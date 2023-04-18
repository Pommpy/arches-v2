#include "unit-cache.hpp"

namespace Arches {namespace Units {

UnitCache::UnitCache(Configuration config) : UnitMemoryBase(config.num_ports),
	incoming_request_network(config.num_ports, config.num_banks),
	outgoing_return_network(config.num_banks, config.num_ports),
	outgoing_request_network(config.num_banks, _mem_map.mappings.size()),
	incoming_return_network(_mem_map.expanded_mappings.size(), config.num_banks)
{
	_configuration = config;

	_associativity = config.associativity;
	_line_size = config.block_size;
	_penalty = config.penalty;

	uint num_blocks = config.size / config.block_size;
	uint num_sets = config.size / (config.block_size * config.associativity);

	uint offset_bits = log2i(config.block_size);
	uint bank_index_bits = log2i(config.num_banks);
	uint set_index_bits = log2i(num_sets);
	uint tag_bits = static_cast<uint>(sizeof(paddr_t) * 8) - (set_index_bits + offset_bits);

	_offset_mask = generate_nbit_mask(offset_bits);

	_set_index_offset = offset_bits;
	_set_index_mask = generate_nbit_mask(set_index_bits);

	_tag_offset = offset_bits + set_index_bits;
	_tag_mask = generate_nbit_mask(tag_bits);

	_bank_index_mask = generate_nbit_mask(bank_index_bits);
	_bank_index_offset = log2i(config.block_size); //line stride for now

	_tag_array.resize(num_blocks);
	_data_array.resize(num_blocks);
	_banks.resize(config.num_banks, {config});

	_mem_map = config.mem_map;
}

UnitCache::~UnitCache()
{

}

//update lru and returns data pointer to cache line
void* UnitCache::_get_block(paddr_t paddr)
{
	uint32_t start = _get_set_index(paddr) * _associativity;
	uint32_t end = start + _associativity;

	uint64_t tag = _get_tag(paddr);

	uint32_t found_index = ~0;
	uint32_t found_lru = 0;
	for(uint32_t i = start; i < end; ++i)
	{
		if(_tag_array[i].valid && _tag_array[i].tag == tag)
		{
			found_index = i;
			found_lru = _tag_array[i].lru;
			break;
		}
	}

	if(found_index == ~0) return nullptr; //didn't find line so we will leave lru alone and return nullptr

	for(uint32_t i = start; i < end; ++i)
		if(_tag_array[i].lru < found_lru) _tag_array[i].lru++;

	_tag_array[found_index].lru = 0;

	return &_data_array[found_index];
}

//inserts cacheline associated with paddr replacing least recently used. Assumes cachline isn't already in cache if it is this has undefined behaviour
void* UnitCache::_insert_block(paddr_t paddr, void* data)
{
	uint32_t start = _get_set_index(paddr) * _associativity;
	uint32_t end = start + _associativity;

	uint32_t replacement_index = ~0u;
	uint32_t replacement_lru = 0u;
	for(uint32_t i = start; i < end; ++i)
	{
		if(!_tag_array[i].valid)
		{
			replacement_index = i;
			break;
		}

		if(_tag_array[i].lru >= replacement_lru)
		{
			replacement_lru = _tag_array[i].lru;
			replacement_index = i;
		}
	}

	for(uint32_t i = start; i < end; ++i)
		_tag_array[i].lru++;

	_tag_array[replacement_index].lru = 0;
	_tag_array[replacement_index].valid = true;
	_tag_array[replacement_index].tag = _get_tag(paddr);

	std::memcpy(_data_array[replacement_index].data, data, CACHE_LINE_SIZE);
	return _data_array[replacement_index].data;
}

UnitCache::Bank::Bank(const UnitCache::Configuration& config)
{
	lsq.resize(config.num_mshr);
}

//allocates a load store entry
bool UnitCache::Bank::_allocate_lse(const MemoryRequest& request, uint64_t request_mask)
{
	paddr_t block_addr = _get_block_addr(request.paddr);
	uint8_t offset = _get_offset(request.paddr);

	uint i;
	for(i = 0; i < lsq.size(); ++i)
	{
		if(lsq[i].state == LSE::FREE)
		{
			lsq[i].state = LSE::VALID;
			lsq[i].block_addr = block_addr;
			lsq[i].offset = offset;
			lsq[i].size = request.size;
			lsq[i].req_mask = request_mask;
		}
	}

	if(i == lsq.size()) return false;

	for(uint j = 0; j < lsq.size(); ++j)
		if(lsq[j].state > lsq[i].state && lsq[j].block_addr == lsq[i].block_addr)
			lsq[i].state = lsq[j].state;

	return true;
}

bool UnitCache::Bank::WCB::get_next(uint8_t& offset, uint8_t& size)
{
	//try to find largest contiguous alligned block to issue load
	size = CACHE_LINE_SIZE;
	while(size > 0)
	{
		uint64_t comb_mask = generate_nbit_mask(size);
		for(offset = 0; offset < CACHE_LINE_SIZE; offset += size)
		{
			if((comb_mask & block_mask) == comb_mask)
			{
				block_mask &= ~(comb_mask);
				return true;
			}

			comb_mask <<= size;
		}
		size /= 2;
	}

	return false;
}

void UnitCache::Bank::clock_rise()
{
	if(incoming_request_mask)
	{
		//incoming request
		if(incoming_request->type == MemoryRequest::Type::LOAD)
		{
			//try to allocate LSE
			if(_allocate_lse(*incoming_request, incoming_request_mask))
			{
				//clear request mask
				incoming_request_mask = 0x0;
			}
		}
		else if(incoming_request->type == MemoryRequest::Type::STORE)
		{
			//try to add to the WCB
			paddr_t block_addr = _get_block_addr(incoming_request->paddr);
			uint offset = _get_offset(incoming_request->paddr);
			if(!wcb.block_mask || block_addr == wcb.block_addr)
			{
				wcb.block_addr = block_addr;
				wcb.block_mask |= generate_nbit_mask(incoming_request->size) << offset;
				std::memcpy(&wcb.data[offset], incoming_request->data, incoming_request->size);

				//clear request mask
				incoming_request_mask = 0x0;
			}
		}
	}

	if(incoming_return_pending)
	{
		//incoming return

	}
}

void UnitCache::Bank::clock_fall()
{
	//fetch next request from the LSQ
	if(lsq[_oldest_valid_lse].state > LSE::ACTIVE)
	{
		//pop
		_oldest_valid_lse += 1;
		_oldest_valid_lse = _oldest_valid_lse % lsq.size();
	}
	if(lsq[_oldest_valid_lse].state == LSE::FREE) return;
	if(lsq[_oldest_valid_lse].state == LSE::VALID)
	{
		cycles_remaining = _penalty;
		lsq[_oldest_valid_lse].state = LSE::ACTIVE;

		//Check if a miss to that line already exists.
		//if yes we already have an outstanding miss so this is a half miss //log.log_half_miss();
		//lsq[_oldest_valid_lse].state = LSE::ISSUED
		//cycles_remaining = 0;
	}
	if(lsq[_oldest_valid_lse].state == LSE::ACTIVE)
	{
		if(cycles_remaining != 0) cycles_remaining--;
		if(cycles_remaining != 0) return;
	}

	LSE& lse = lsq[_oldest_valid_lse];

	//Check if we have the block
	void* block_data = _get_block(lse.block_addr);
	if(block_data)
	{
		//log.log_hit();
		lse.size = LSE::READY;
		//mark as ready to return
	}
	else
	{
		//try to alloacte a new MSHR. //log.log_miss();

		//if mshr is full try again next cycle //log.log_mshr_stall();

	}

	//try to return data
	//try to request the next line
}

void UnitCache::_route_incoming_requests()
{
	//route ports to arbitrators
	for(uint port_index = 0; port_index < request_bus.size(); ++port_index)
	{
		if(!request_bus.get_pending(port_index)) continue;

		const MemoryRequest& request = request_bus.get_data(port_index);
		uint32_t bank_index = _get_bank_index(request.paddr);
		incoming_request_network.add(port_index, bank_index);
	}

	//update network
	incoming_request_network.update();

	//arbitrate
	for(uint bank_index = 0; bank_index < _banks.size(); ++bank_index)
	{
		Bank& bank = _banks[bank_index];
		bank.incoming_request_port = incoming_request_network.input(bank_index);
	}
}

void UnitCache::_route_incoming_returns()
{
	//route ports to arbitrators
	for(uint port_index = 0; port_index < _mem_map.expanded_mappings.size(); ++port_index)
	{
		MemoryUnitMap::MemoryUnitMapping mapping = _mem_map.expanded_mappings[port_index];
		if(!mapping.unit->return_bus.get_pending(mapping.port_id)) continue;

		//if there is pending request try map it to to the correct bank
		const MemoryRequest& request = mapping.unit->return_bus.get_data(mapping.port_id);
		uint32_t bank_index = _get_bank_index(request.paddr);
		incoming_return_network.add(port_index, bank_index);
	}

	//update network
	incoming_return_network.update();

	//arbitrate
	for(uint bank_index = 0; bank_index < _banks.size(); ++bank_index)
	{
		Bank& bank = _banks[bank_index];
		bank.incoming_return_port = incoming_return_network.input(bank_index);
	}
}

void UnitCache::_route_outgoing_requests()
{
	//update network
	outgoing_request_network.update();
}

void UnitCache::_route_outgoing_returns()
{
	//update network
	outgoing_return_network.update();
}

void UnitCache::clock_rise()
{
	_route_incoming_returns();
	_route_incoming_requests();

	for(uint i = 0; i < _banks.size(); ++i)
		_banks[i].clock_rise();

	//todo propagate outgoing ack
}

void UnitCache::clock_fall()
{
	for(uint i = 0; i < _banks.size(); ++i)
		_banks[i].clock_fall();

	_route_outgoing_requests();
	_route_outgoing_returns();

	_banks[i].send_outputs();

	//todo propagate incoming ack
}

}}
#include "unit-l1-cache.hpp"

namespace Arches {namespace Units {

UnitL1Cache::UnitL1Cache(Configuration config) : UnitCacheBase(config.size, config.associativity),
	_request_cross_bar(config.num_ports, config.num_banks),
	_return_cross_bar(config.num_banks, config.num_ports),
	outgoing_request_network(config.num_banks, config.mem_map.total_ports),
	incoming_return_network(config.mem_map.total_ports, config.num_banks)
{
	_mem_map = config.mem_map;

	_configuration = config;

	_banks.resize(config.num_banks, {config.num_lfb});

	_data_array_access_cycles = config.data_array_access_cycles;
	_tag_array_access_cycles = config.tag_array_access_cycles;

	uint bank_index_bits = log2i(config.num_banks);
	_bank_index_mask = generate_nbit_mask(bank_index_bits);
	_bank_index_offset = log2i(CACHE_BLOCK_SIZE); //line stride for now
}

UnitL1Cache::~UnitL1Cache()
{

}

uint UnitL1Cache::_fetch_lfb(uint bank_index, _LFB& lfb)
{
	_Bank& bank = _banks[bank_index];
	std::vector<_LFB>& lfbs = bank.lfbs;

	for(uint32_t i = 0; i < lfbs.size(); ++i)
		if(lfbs[i].state != _LFB::State::INVALID && lfbs[i] == lfb) return i;

	return ~0u;
}

uint UnitL1Cache::_allocate_lfb(uint bank_index, _LFB& lfb)
{
	std::vector<_LFB>& lfbs = _banks[bank_index].lfbs;

	uint replacement_index = ~0u;
	uint replacement_lru = 0u;
	for(uint i = 0; i < lfbs.size(); ++i)
	{
		if(lfbs[i].state == _LFB::State::INVALID)
		{
			replacement_index = i;
			break;
		}

		if(lfbs[i].state == _LFB::State::RETIRED && lfbs[i].lru >= replacement_lru)
		{
			replacement_lru = lfbs[i].lru;
			replacement_index = i;
		}
	}

	//can't allocate
	if(replacement_index == ~0) return ~0;

	for(uint i = 0; i < lfbs.size(); ++i) lfbs[i].lru++;
	lfbs[replacement_index].lru = 0;
	lfbs[replacement_index] = lfb;
	return replacement_index;
}

uint UnitL1Cache::_fetch_or_allocate_lfb(uint bank_index, uint64_t block_addr, _LFB::Type type)
{
	std::vector<_LFB>& lfbs = _banks[bank_index].lfbs;

	_LFB lfb;
	lfb.block_addr = block_addr;
	lfb.type = type;
	lfb.state = _LFB::State::EMPTY;
	
	uint lfb_index = _fetch_lfb(bank_index, lfb);
	if(lfb_index != ~0) return lfb_index;
	return _allocate_lfb(bank_index, lfb);
}

void UnitL1Cache::_push_request(_LFB& lfb, const MemoryRequest& request, uint port)
{
	_LFB::SubEntry sub_entry;
	sub_entry.offset = _get_block_offset(request.paddr);
	sub_entry.size = request.size;
	sub_entry.dst = request.dst;
	sub_entry.port = port;
	lfb.sub_entries.push(sub_entry);
}

MemoryRequest UnitL1Cache::_pop_request(_LFB& lfb, uint& port)
{
	_LFB::SubEntry sub_entry = lfb.sub_entries.front();
	lfb.sub_entries.pop();

	port = sub_entry.port;

	MemoryRequest req;
	req.type = MemoryRequest::Type::LOAD;
	req.size = sub_entry.size;
	req.dst = sub_entry.dst;
	req.paddr = lfb.block_addr + sub_entry.offset;
	return req;
}

void UnitL1Cache::_interconnects_rise()
{
	//route ports to arbitrators
	for(uint mapping_index = 0; mapping_index < _mem_map.mappings.size(); ++mapping_index)
	{
		MemoryMap::MemoryMapping mapping = _mem_map.mappings[mapping_index];
		for(uint j = 0; j < mapping.num_ports; ++j, ++mapping.port_index, ++mapping.port_id)
		{
			if(!mapping.unit->return_port_read_valid(mapping.port_index)) continue;
			const MemoryReturn& ret = mapping.unit->peek_return(mapping.port_index);

			//if there is pending request try map it to to the correct bank
			uint bank_index = _get_bank_index(ret.paddr);
			incoming_return_network.add(mapping.port_id, bank_index);
		}
	}
}

bool UnitL1Cache::_proccess_return(uint bank_index)
{
	uint port_id = incoming_return_network.src(bank_index);
	if(port_id == ~0) return false;

	uint mapping_index = _mem_map.get_mapping_index_for_unique_port_index(port_id);
	MemoryMap::MemoryMapping mapping = _mem_map.mappings[mapping_index];
	uint port_index = mapping.port_index + (port_id - mapping.port_id);

	const MemoryReturn& ret = mapping.unit->peek_return(port_index);
	assert(ret.paddr == _get_block_addr(ret.paddr));

	//Mark the associated lse as filled and put it in the return queue
	_Bank& bank = _banks[bank_index];
	for(uint i = 0; i < bank.lfbs.size(); ++i)
	{
		_LFB& lfb = bank.lfbs[i];
		if(lfb.block_addr == ret.paddr)
		{ 
			lfb.block_data = *((_BlockData*)ret.data);
			lfb.state = _LFB::State::FILLED;
			bank.lfb_return_queue.push(i);
		}
	}

	//Insert block
	log.log_tag_array_access();
	log.log_data_array_write();
	_insert_block(ret.paddr, *(_BlockData*)ret.data);

	mapping.unit->read_return(port_index);
	incoming_return_network.remove(port_id, bank_index);

	return true;
}

bool UnitL1Cache::_proccess_request(uint bank_index)
{
	if(!_request_cross_bar.is_read_valid(bank_index)) return false;

	uint port_index;
	const MemoryRequest& request = _request_cross_bar.peek(bank_index, port_index);
	paddr_t block_addr = _get_block_addr(request.paddr);
	uint block_offset = _get_block_offset(request.paddr);
	log.log_requests();

	_Bank& bank = _banks[bank_index];
	if(request.type == MemoryRequest::Type::LOAD)
	{
		//Try to fetch an lfb for the line or allocate a new lfb for the line
		uint lfb_index = _fetch_or_allocate_lfb(bank_index, block_addr, _LFB::Type::READ);

		//In parallel access the tag array to check for the line
		_BlockData* block_data = _get_block(block_addr);
		log.log_tag_array_access();

		if(lfb_index != ~0)
		{
			_LFB& lfb = bank.lfbs[lfb_index];
			_push_request(lfb, request, port_index);

			if(lfb.state == _LFB::State::EMPTY)
			{
				if(block_data)
				{
					//Copy line from data array to LFB
					lfb.block_data = *block_data;
					log.log_data_array_read();
					
					lfb.state = _LFB::State::FILLED;
					bank.lfb_return_queue.push(lfb_index);
					log.log_hit();
				}
				else
				{
					lfb.state = _LFB::State::MISSED;
					bank.lfb_request_queue.push(lfb_index);
					log.log_miss();
				}
			}
			else if(lfb.state == _LFB::State::MISSED)
			{
				log.log_half_miss();
			}
			else if(lfb.state == _LFB::State::ISSUED)
			{
				log.log_half_miss();
			}
			else if(lfb.state == _LFB::State::FILLED)
			{
				log.log_lfb_hit();
			}
			else if(lfb.state == _LFB::State::RETIRED)
			{
				//Wake up retired LFB and add it to the return queue
				lfb.state = _LFB::State::FILLED;
				bank.lfb_return_queue.push(lfb_index);
				log.log_lfb_hit();
			}

			_request_cross_bar.read(bank_index);
		}
		else log.log_lfb_stall();
	}
	else if(request.type == MemoryRequest::Type::STORE)
	{
		//try to allocate an lfb
		uint lfb_index = _fetch_or_allocate_lfb(bank_index, block_addr, _LFB::Type::WRITE_COMBINING);
		if(lfb_index != ~0)
		{
			_LFB& lfb = bank.lfbs[lfb_index];
			lfb.write_mask |= generate_nbit_mask(request.size) << block_offset;
			std::memcpy(&lfb.block_data.bytes[block_offset], request.data, request.size);
			
			if(lfb.state == _LFB::State::EMPTY)
			{
				lfb.state = _LFB::State::FILLED; //since we just filled it
				bank.lfb_request_queue.push(lfb_index);
			}

			_request_cross_bar.read(bank_index);
		}
	}

	if(_request_cross_bar.is_read_valid(bank_index)) log.log_bank_conflict(); //log bank conflicts

	return true;
}

void UnitL1Cache::_propagate_ack()
{
	//outgoing requests
	for(uint mapping_index = 0; mapping_index < _mem_map.mappings.size(); ++mapping_index)
	{
		MemoryMap::MemoryMapping mapping = _mem_map.mappings[mapping_index];
		for(uint i = 0; i < mapping.num_ports; ++i, ++mapping.port_index, ++mapping.port_id)
		{
			uint bank_index = outgoing_request_network.src(mapping.port_id);
			if(bank_index == ~0 || !mapping.unit->request_port_write_valid(mapping.port_index)) continue;

			_Bank& bank = _banks[bank_index];
			_LFB& lfb = bank.lfbs[bank.lfb_request_queue.front()];
			if(lfb.type == _LFB::Type::READ)
			{
				lfb.state = _LFB::State::ISSUED;
				bank.lfb_request_queue.pop();
				outgoing_request_network.remove(bank_index, mapping.port_id);
			}
			else if(lfb.type == _LFB::Type::WRITE_COMBINING)
			{
				//clear the bytes we sent from the byte mask
				lfb.write_mask &= ~bank.outgoing_write_mask;
				if(!lfb.write_mask)
				{
					lfb.state = _LFB::State::INVALID; //invalidate when we are done
					bank.lfb_request_queue.pop();
					outgoing_request_network.remove(bank_index, mapping.port_id); //only clear if we are finished using the bus
				}
			}
		}
	}
}

void UnitL1Cache::_try_request_lfb(uint bank_index)
{
	_Bank& bank = _banks[bank_index];

	if(bank.lfb_request_queue.empty()) return;
	_LFB& lfb = bank.lfbs[bank.lfb_request_queue.front()];

	uint mapping_index = _mem_map.get_mapping_index(lfb.block_addr);
	MemoryMap::MemoryMapping mapping = _mem_map.mappings[mapping_index];
	uint port_id = mapping.port_id + (bank_index % mapping.num_ports);
	outgoing_request_network.add(bank_index, port_id);
}

void UnitL1Cache::_try_return_lfb(uint bank_index)
{
	_Bank& bank = _banks[bank_index];

	//last return isn't complete do nothing
	if(!_return_cross_bar.is_write_valid(bank_index)) return;
	if(bank.lfb_return_queue.empty()) return;

	_LFB& lfb = bank.lfbs[bank.lfb_return_queue.front()];

	//Select the next subentry and copy return to interconnect
	uint port_index;
	MemoryReturn ret = _pop_request(lfb, port_index);
	std::memcpy(ret.data, &lfb.block_data.bytes[_get_block_offset(ret.paddr)], ret.size);
	_return_cross_bar.write(ret, bank_index, port_index);

	if(lfb.sub_entries.empty())
	{
		lfb.state = _LFB::State::RETIRED;
		bank.lfb_return_queue.pop();
	}
}

void UnitL1Cache::_interconnects_fall()
{
	//copy requests from lfb
	for(uint mapping_index = 0; mapping_index < _mem_map.mappings.size(); ++mapping_index)
	{
		MemoryMap::MemoryMapping mapping = _mem_map.mappings[mapping_index];
		for(uint i = 0; i < mapping.num_ports; ++i, ++mapping.port_index, ++mapping.port_id)
		{
			uint bank_index = outgoing_request_network.src(mapping.port_id);
			if(bank_index == ~0u || !mapping.unit->request_port_write_valid(mapping.port_index)) continue; //either nothing is pending or we already issued

			//add transfer
			_Bank& bank = _banks[bank_index];

			_LFB& lfb = bank.lfbs[bank.lfb_request_queue.front()];
			MemoryRequest outgoing_request;
			if(lfb.type == _LFB::Type::READ)
			{
				outgoing_request.size = CACHE_BLOCK_SIZE;
				outgoing_request.paddr = lfb.block_addr;
				outgoing_request.type = MemoryRequest::Type::LOAD;
			}
			else if(lfb.type == _LFB::Type::WRITE_COMBINING)
			{
				uint8_t offset, size;
				if(lfb.get_next_write(offset, size))
				{
					outgoing_request.type = MemoryRequest::Type::STORE;
					outgoing_request.size = size;
					outgoing_request.paddr = lfb.block_addr + offset;
					std::memcpy(outgoing_request.data, &lfb.block_data.bytes[offset], size);
					bank.outgoing_write_mask = generate_nbit_mask(size) << offset;
				}
				//todo if our transfer is pending and we have new data in the bufffer we should update it
			}

			mapping.unit->write_request(outgoing_request, mapping.port_index);
		}
	}
}

void UnitL1Cache::clock_rise()
{
	_interconnects_rise();

	for(uint i = 0; i < _banks.size(); ++i)
	{
		//if we select a return it will access the data array and lfb so we can't accept a request on this cycle
		if(!_proccess_return(i))
			_proccess_request(i);
	}
}

void UnitL1Cache::clock_fall()
{
	_propagate_ack();

	for(uint i = 0; i < _banks.size(); ++i)
	{
		_try_request_lfb(i);
		_try_return_lfb(i);
	}

	_interconnects_fall();
}

bool UnitL1Cache::request_port_write_valid(uint port_index)
{
	return _request_cross_bar.is_write_valid(port_index);
}

void UnitL1Cache::write_request(const MemoryRequest& request, uint port_index)
{
	uint bank_index = _get_bank_index(request.paddr);
	_request_cross_bar.write(request, port_index, bank_index);
}

bool UnitL1Cache::return_port_read_valid(uint port_index)
{
	return _return_cross_bar.is_read_valid(port_index);
}

const MemoryReturn& UnitL1Cache::peek_return(uint port_index)
{
	return _return_cross_bar.peek(port_index);
}

const MemoryReturn& UnitL1Cache::read_return(uint port_index)
{
	return _return_cross_bar.read(port_index);
}

}}
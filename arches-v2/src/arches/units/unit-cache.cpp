#include "unit-cache.hpp"

namespace Arches {namespace Units {

UnitCache::UnitCache(Configuration config) : UnitMemoryBase(config.num_ports)
{
	_mem_map = config.mem_map;

	_configuration = config;

	_tag_array.resize(config.size / config.block_size);
	_data_array.resize(config.size / config.block_size);
	_banks.resize(config.num_banks, {config.num_mshr});

	_num_ports = config.num_ports;

	_associativity = config.associativity;
	_line_size = config.block_size;
	_penalty = config.penalty;

	uint num_sets = config.size / (config.block_size * config.associativity);

	uint offset_bits = log2i(config.block_size);
	uint set_index_bits = log2i(num_sets);
	uint tag_bits = static_cast<uint>(sizeof(paddr_t) * 8) - (set_index_bits + offset_bits);
	uint bank_index_bits = log2i(config.num_banks);

	offset_mask = generate_nbit_mask(offset_bits);
	_set_index_mask = generate_nbit_mask(set_index_bits);
	_tag_mask = generate_nbit_mask(tag_bits);
	_bank_index_mask = generate_nbit_mask(bank_index_bits);

	_set_index_offset = offset_bits;
	_tag_offset = offset_bits + set_index_bits;
	_bank_index_offset = log2i(config.block_size); //line stride for now
}

UnitCache::~UnitCache()
{

}

uint UnitCache::_Bank::get_mshr(paddr_t line_paddr, uint64_t req_mask)
{
	for(uint i = 0; i < mshrs.size(); ++i)
		if(mshrs[i].state != _MSHR::FREE && mshrs[i].block_addr == line_paddr)
		{
			mshrs[i].request_mask |= req_mask;
			return i;
		}

	return ~0u;
}

uint UnitCache::_Bank::allocate_mshr(paddr_t line_paddr, uint64_t req_mask)
{
	for(uint i = 0; i < mshrs.size(); ++i)
		if(mshrs[i].state == _MSHR::FREE)
		{
			mshrs[i].block_addr = line_paddr;
			mshrs[i].request_mask = req_mask;
			mshrs[i].state = _MSHR::VALID;
			return i;
		}

	//failed to allocate mshrs are full
	return ~0u;
}

void UnitCache::_Bank::free_mshr(uint index)
{
	uint i;
	for(i = index; i < (mshrs.size() - 1); ++i)
		mshrs[i] = mshrs[i + 1];

	mshrs[i].state = _MSHR::FREE;
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

void UnitCache::_try_return_load(const MemoryRequest& rtrn, uint64_t& mask)
{

}

void UnitCache::_proccess_returns()
{
	for(uint i = 0; i < _mem_map.units.size(); ++i)
	{
		UnitMemoryBase* mem_higher = _mem_map.get_unit(i);
		uint port = _mem_map.get_port(i);

		if(!mem_higher->return_bus.get_pending(port)) continue;

		const MemoryRequest& rtrn = mem_higher->return_bus.get_data(port);

		paddr_t block_addr = _get_block_addr(rtrn.paddr);
		uint bank_index = _get_bank(rtrn.paddr);
		_Bank& bank = _banks[bank_index];

		uint mshr_index = bank.get_mshr(block_addr);
		_MSHR& mshr = bank.mshrs[mshr_index];

		uint64_t req_mask = mshr.request_mask;

		if(bank.busy)
		{
			//can't proccess on this cycle
			if(!bank.stalled_for_mshr) return;

			//otherwise we are stalled for mshr so we swap the current reuest with the returned mshr and return the data to mem lower

			bank.free_mshr(mshr_index);
			mshr_index = bank.allocate_mshr(_get_block_addr(bank.request.paddr), bank.request_mask);
			bank.stalled_for_mshr = false;
		}
		else
		{
			bank.free_mshr(mshr_index);
		}

		bank.request.paddr = block_addr;
		bank.request.size = _line_size;
		bank.request.type = MemoryRequest::Type::LOAD_RETURN;
		bank.request_mask = req_mask;

		//activate bank to return data this cycle
		bank.cycles_remaining = 0;
		bank.busy = true;

		mem_higher->return_bus.clear_pending(port);
		bank.request.data = _insert_block(block_addr, rtrn.data);
	}
}

void UnitCache::_proccess_requests()
{
	uint request_priority_index = request_bus.get_next_pending();
	if(request_priority_index == ~0u) return;

	//loop till all entries incoming connection have been checked
	for(uint i = 0; i < request_bus.size(); ++i)
	{
		uint request_index = (request_priority_index + i) % request_bus.size();
		if(!request_bus.get_pending(request_index)) continue;

		const MemoryRequest& request = request_bus.get_data(request_index);
		uint32_t bank_index = _get_bank(request.paddr);
		_Bank& bank = _banks[bank_index];

		if(request.type == MemoryRequest::Type::STORE) //place stores directly in WC. since they are uncached we dont need to aquire the bank
		{
			paddr_t block_addr = _get_block_addr(request.paddr);
			uint offset = request.paddr - block_addr;

			if(!bank.wcb.mask || block_addr == bank.wcb.block_addr)
			{
				bank.wcb.block_addr = block_addr;
				bank.wcb.mask |= generate_nbit_mask(request.size) << offset;
				std::memcpy(&bank.wcb.data[offset], request.data, request.size);
				request_bus.clear_pending(request_index);
			}
		}
		else
		{
			if(!bank.busy)
			{
				//if bank isn't busy and there are no pending requests to that bank then we can skip it
				bank.cycles_remaining = _penalty;
				bank.busy = true;

				bank.request = request;
				bank.request_mask = 0x1ull << request_index;
				request_bus.clear_pending(request_index);
			}
			else
			{
				if((bank.request.type == MemoryRequest::Type::LOAD || bank.request.type == MemoryRequest::Type::LOAD_RETURN) &&
					request.type == MemoryRequest::Type::LOAD && request.paddr == bank.request.paddr && request.size == bank.request.size)
				{
					//merge requests to the same address
					bank.request_mask |= 0x1ull << request_index;
					request_bus.clear_pending(request_index);
				}
				else
				{
					log.log_bank_conflict();
				}
			}
		}
	}
}

void UnitCache::_update_bank(uint bank_index)
{
	_Bank& bank = _banks[bank_index];
	if(!bank.busy) return;

	if(bank.request.type == MemoryRequest::Type::STORE)
	{
		//stores go around cache for now
		assert(false);
	}
	else if(bank.request.type == MemoryRequest::Type::LOAD)
	{
		if(bank.cycles_remaining > 0) bank.cycles_remaining--;
		if(bank.cycles_remaining != 0) return;

		//Check if we have the block
		paddr_t block_addr = _get_block_addr(bank.request.paddr);
		void* block_data = _get_block(block_addr);
		if(block_data)
		{
			log.log_hit();

			//Try to return data
			bank.request.type = MemoryRequest::Type::LOAD_RETURN;
			bank.request.data = block_data;
		}
		else
		{
			//Miss. Check if the miss already exists.
			uint mshr_index = bank.get_mshr(block_addr, bank.request_mask);
			if(mshr_index != ~0u)
			{
				log.log_half_miss(); //we already have an outstanding miss so this is a half miss
				bank.busy = false; //free bank
				return;
			}

			//Try to alloacte a new MSHR
			mshr_index = bank.allocate_mshr(block_addr, bank.request_mask);
			if(mshr_index != ~0u)
			{
				log.log_miss();
				bank.busy = false; //free bank
				return;
			}

			//Stall since mshr is full (try again next cycle)
			log.log_mshr_stall();
			bank.stalled_for_mshr = true;
		}
	}
}

void UnitCache::_try_issue_load_returns(uint bank_index)
{
	//Try to return data for the line we just recived
	_Bank& bank = _banks[bank_index];
	if(bank.request.type == MemoryRequest::Type::LOAD_RETURN)
	{
		//return to all pending ports
		for(uint i = 0; i < return_bus.size(); ++i)
		{
			uint64_t imask = (0x1ull << i);
			if((bank.request_mask & imask) == 0x0ull || return_bus.get_pending(i)) continue;

			return_bus.set_data(bank.request, i);
			return_bus.set_pending(i);

			bank.request_mask &= ~imask;
		}

		//If we succed to return to all clients we free the bank otherwise we will try again next cycle
		if(bank.request_mask == 0x0ull) bank.busy = false;
	}
}

void UnitCache::_try_issue_loads(uint bank_index)
{
	_Bank& bank = _banks[bank_index];

	for(uint mshr_index = 0; mshr_index < bank.mshrs.size(); ++mshr_index)
	{
		if(bank.mshrs[mshr_index].state != _MSHR::VALID) continue;

		uint mem_higher_index = _mem_map.get_index(bank.mshrs[mshr_index].block_addr);
		UnitMemoryBase* mem_higher = _mem_map.get_unit(mem_higher_index);
		uint port = _mem_map.get_port(mem_higher_index);
		if(mem_higher->request_bus.get_pending(port)) continue;

		MemoryRequest request;
		request.type = MemoryRequest::Type::LOAD;
		request.size = _line_size;
		request.paddr = bank.mshrs[mshr_index].block_addr;

		mem_higher->request_bus.set_data(request, port);
		mem_higher->request_bus.set_pending(port);

		bank.mshrs[mshr_index].state = _MSHR::ISSUED;

		break;
	}
}

void UnitCache::_try_issue_stores(uint bank_index)
{
	_Bank& bank = _banks[bank_index];

	if(bank.wcb.mask)
	{
		uint mem_higher_index = _mem_map.get_index(bank.wcb.block_addr);
		UnitMemoryBase* mem_higher = _mem_map.get_unit(mem_higher_index);
		uint port = _mem_map.get_port(mem_higher_index);
		if(mem_higher->request_bus.get_pending(port)) return;

		uint8_t offset;
		MemoryRequest request;
		bank.wcb.get_next(offset, request.size);
		request.type = MemoryRequest::Type::STORE;
		request.paddr = bank.wcb.block_addr + offset;
		request.data = &bank.wcb.data[offset];

		mem_higher->request_bus.set_data(request, port);
		mem_higher->request_bus.set_pending(port);
	}
}

void UnitCache::clock_rise()
{
	_proccess_returns();
	_proccess_requests();
}

void UnitCache::clock_fall()
{
	for(uint i = 0; i < _banks.size(); ++i)
		_update_bank(i);

	//TODO proper arbitration
	for(uint i = 0; i < _banks.size(); ++i)
	{
		uint bank_index = (_bank_priority_index + i) % _banks.size();
		_try_issue_load_returns(bank_index);
		_try_issue_loads(bank_index);
		_try_issue_stores(bank_index);
	}
	_bank_priority_index = (_bank_priority_index + 1) % _banks.size();
}

}}
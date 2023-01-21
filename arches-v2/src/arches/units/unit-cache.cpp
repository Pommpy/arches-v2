#include "unit-cache.hpp"

namespace Arches {
	namespace Units {

		UnitCache::UnitCache(Configuration config, UnitMemoryBase* mem_higher, uint mem_higher_request_index, Simulator* simulator) : UnitMemoryBase(config.num_incoming_connections, simulator)
		{
			_configuration = config;

			_blocking = config.blocking;

			_cache_state.resize(config.cache_size / config.line_size);
			_banks.resize(config.num_banks, {config.num_mshr});

			_mem_higher = mem_higher;
			_mem_higher_request_index = mem_higher_request_index;
			
			_num_incoming_connections = config.num_incoming_connections;

			_associativity = config.associativity;
			_line_size = config.line_size;
			_penalty = config.penalty;

			uint num_sets = config.cache_size / (config.line_size * config.associativity);

			offset_bits = log2i(config.line_size);
			uint set_index_bits = log2i(num_sets);
			uint tag_bits = static_cast<uint>(sizeof(paddr_t) * 8) - (set_index_bits + offset_bits);
			uint bank_index_bits = log2i(config.num_banks);

			offset_mask = generate_nbit_mask(offset_bits);
			_set_index_mask = generate_nbit_mask(set_index_bits);
			_tag_mask = generate_nbit_mask(tag_bits);
			_bank_index_mask = generate_nbit_mask(bank_index_bits);

			_set_index_offset = offset_bits;
			_tag_offset = offset_bits + set_index_bits;
			_bank_index_offset = log2i(config.line_size); //line stride for now
		}

		UnitCache::~UnitCache()
		{

		}

		//update lru and returns data pointer to cache line
		bool UnitCache::_get_cache_line(paddr_t paddr)
		{
			uint32_t start = _get_set_index(paddr) * _associativity;
			uint32_t end = start + _associativity;

			uint64_t tag = _get_tag(paddr);

			uint32_t found_index = ~0;
			uint32_t found_lru = 0;
			for(uint32_t i = start; i < end; ++i)
			{
				if(_cache_state[i].valid && _cache_state[i].tag == tag)
				{
					found_index = i;
					found_lru = _cache_state[i].lru;
					break;
				}
			}

			if(found_index == ~0) return false; //didn't find line so we will leave lru alone and return nullptr

			for(uint32_t i = start; i < end; ++i)
				if(_cache_state[i].lru < found_lru) _cache_state[i].lru++;

			_cache_state[found_index].lru = 0;

			return true;
		}

		//inserts cacheline associated with paddr replacing least recently used. Assumes cachline isn't already in cache if it is this has undefined behaviour
		void UnitCache::_insert_cache_line(paddr_t paddr)
		{
			uint32_t start = _get_set_index(paddr) * _associativity;
			uint32_t end = start + _associativity;

			uint32_t replacement_index = ~0u;
			uint32_t replacement_lru = 0u;
			for(uint32_t i = start; i < end; ++i)
			{
				if(!_cache_state[i].valid)
				{
					replacement_index = i;
					break;
				}
				
				if(_cache_state[i].lru >= replacement_lru)
				{
					replacement_lru = _cache_state[i].lru;
					replacement_index = i;
				}
			}

			for(uint32_t i = start; i < end; ++i)
				_cache_state[i].lru++;

			_cache_state[replacement_index].lru = 0;
			_cache_state[replacement_index].valid = true;
			_cache_state[replacement_index].tag = _get_tag(paddr);
		}

		void UnitCache::_try_return(const MemoryRequest& rtrn, uint64_t& mask)
		{
			assert(rtrn.type == MemoryRequest::Type::LOAD_RETURN);
			assert(rtrn.size == CACHE_LINE_SIZE);
			assert(rtrn.paddr == _get_block_addr(rtrn.paddr));

			//return to all pending ports
			for(uint i = 0; i < return_bus.size(); ++i)
			{
				uint64_t imask = (0x1ull << i);
				if((mask & imask) == 0x0ull || return_bus.get_pending(i)) continue;

				return_bus.set_data(rtrn, i);
				return_bus.set_pending(i);

				mask &= ~imask;
			}
		}

		void UnitCache::_proccess_load_return()
		{
			if(!_mem_higher->return_bus.get_pending(_mem_higher_request_index)) return;

			const MemoryRequest& rtrn = _mem_higher->return_bus.get_data(_mem_higher_request_index);

			paddr_t block_addr = _get_block_addr(rtrn.paddr);
			uint bank_index = _get_bank(rtrn.paddr);
			_Bank& bank = _banks[bank_index];

			uint mshr_index = bank.get_mshr(block_addr);
			MSHR& mshr = bank.mshrs[mshr_index];

			uint64_t req_mask = mshr.request_mask;

			if(_blocking)
			{
				//TODO blocking version
			}
			else
			{
				if(bank.busy)
				{
					//can't proccess on this cycle
					if(!bank.stalled_for_mshr) return;

					//othweise we are stalled for mshr so we swap the current reuest with the returned mshr and return the data to mem lower

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

				_mem_higher->return_bus.clear_pending(_mem_higher_request_index);
				_insert_cache_line(block_addr);
			}
		}

		void UnitCache::_proccess_requests()
		{
			//loop till all entries incoming connection have been checked
			
			for(uint i = 0; i < _num_incoming_connections; ++i)
			{
				uint request_index = (_port_priority_index + i) % _num_incoming_connections;
				if(!request_bus.get_pending(request_index)) continue;

				const MemoryRequest& request = request_bus.get_data(request_index);
				uint32_t bank_index = _get_bank(request.paddr);
				_Bank& bank = _banks[bank_index];

				if(request.type == MemoryRequest::Type::STORE) //place stores directly in WC. since they are uncacheed we dont need to aquire the bank
				{
					paddr_t block_addr = _get_block_addr(request.paddr);
					uint offset = request.paddr - block_addr;

					if(!bank.wcb.mask || block_addr == bank.wcb.block_addr)
					{
						bank.wcb.block_addr = block_addr;
						bank.wcb.mask |= generate_nbit_mask(request.size) << offset;
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
						if(request.type == MemoryRequest::Type::LOAD) assert(request.size == CACHE_LINE_SIZE);

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

			_port_priority_index = (_port_priority_index + 1) % _num_incoming_connections;
		}

		void UnitCache::_update_banks_fall_blocking()
		{
			assert(false);
		#if 0
			//TODO FIX
			for(uint bank_index = 0; bank_index < _banks.size(); ++bank_index)
			{
				_Bank& bank = _banks[bank_index];
				if(!bank.busy || bank.block_for_load) continue;

				if(bank.block_for_store)
				{
					//check if last request has been accepted
					if(!_mem_higher->request_bus.get_pending(_mem_higher_request_index))
					{
						bank.block_for_store = false;
						bank.busy = false;
					}
					continue;
				}

				if(bank.request.type == MemoryRequest::Type::STORE)
				{
					//forward store
					_mem_higher->request_bus.set_data(bank.request, _mem_higher_request_index);
					_mem_higher->request_bus.set_pending(_mem_higher_request_index);
					bank.block_for_store = true;
					continue;
				}
				else if(bank.request.type == MemoryRequest::Type::LOAD)
				{
					if(bank.cycles_remaining > 0) bank.cycles_remaining--;
					if(bank.cycles_remaining != 0) continue;

					//check if we have the data
					paddr_t block_addr = _get_block_addr(bank.request.paddr);
					if(_get_cache_line(block_addr))
					{
						log.log_hit();

						//if we do have the data return it to pending units and release bank if we did
						bank.request.type = MemoryRequest::Type::LOAD_RETURN;
						_try_return(bank.request, bank.request_mask);

						//free bank if we were able to return all requests else stall
						if(bank.request_mask != 0ull) bank.busy = false;
					}
					else
					{
						//issue to mem_higher and stall
						if(!_mem_higher->request_bus.get_pending(_mem_higher_request_index))
						{
							log.log_miss();

							MemoryRequest request;
							request.type = MemoryRequest::Type::LOAD;
							request.size = CACHE_LINE_SIZE;
							request.paddr = _get_block_addr(bank.request.paddr);

							_mem_higher->request_bus.set_data(request, _mem_higher_request_index);
							_mem_higher->request_bus.set_pending(_mem_higher_request_index);
							bank.block_for_load = true;
						}
						else
						{
							bank.cycles_remaining = 1;
						}
					}
				}
			}
		#endif
		}

		void UnitCache::_update_banks_fall()
		{
			for(uint i = 0; i < _banks.size(); ++i)
			{
				uint bank_index = (_bank_priority_index + i) % _banks.size();
				_Bank& bank = _banks[bank_index];
				if(!bank.busy) continue;

				if(bank.request.type == MemoryRequest::Type::STORE)
				{
					assert(false);
				}
				else if(bank.request.type == MemoryRequest::Type::LOAD)
				{
					if(bank.cycles_remaining > 0) bank.cycles_remaining--;
					if(bank.cycles_remaining != 0) continue;

					//check if we have the data
					paddr_t block_addr = _get_block_addr(bank.request.paddr);
					if(_get_cache_line(block_addr))
					{
						log.log_hit();

						//try to return data
						bank.request.type = MemoryRequest::Type::LOAD_RETURN;
						_try_return(bank.request, bank.request_mask);

						//if we succed to return to all clients we free the bank otherwise we will try again next cycle
						if(bank.request_mask == 0x0ull) bank.busy = false;
						continue;
					}

					uint mshr_index = bank.get_mshr(block_addr, bank.request_mask);
					if(mshr_index != ~0u)
					{
						log.log_half_miss(); //we already have an outstanding miss so this is a half miss
						bank.busy = false; //free bank
						continue;
					}

					mshr_index = bank.allocate_mshr(block_addr, bank.request_mask);
					if(mshr_index != ~0u)
					{
						log.log_miss();
						bank.busy = false; //free bank
						continue;
					}

					//stall since mshr is full (try again next cycle)
					log.log_mshr_stall();
					bank.stalled_for_mshr = true;
				}
				else if(bank.request.type == MemoryRequest::Type::LOAD_RETURN)
				{
					//try to return data for the line we just recived
					_try_return(bank.request, bank.request_mask);
					if(bank.request_mask == 0x0ull) bank.busy = false;
				}
			}

			if(!_mem_higher->request_bus.get_pending(_mem_higher_request_index))
			{
				for(uint i = 0; i < _banks.size(); ++i)
				{
					uint bank_index = (_bank_priority_index + i) % _banks.size();
					_Bank& bank = _banks[bank_index];

					//try to issue next mshr
					for(uint mshr_index = 0; mshr_index < bank.mshrs.size(); ++mshr_index)
					{
						if(bank.mshrs[mshr_index].state != MSHR::VALID) continue;

						MemoryRequest request;
						request.type = MemoryRequest::Type::LOAD;
						request.size = _line_size;
						request.paddr = bank.mshrs[mshr_index].block_addr;

						_mem_higher->request_bus.set_data(request, _mem_higher_request_index);
						_mem_higher->request_bus.set_pending(_mem_higher_request_index);

						bank.mshrs[mshr_index].state = MSHR::ISSUED;
						goto UPDATE_BANK_PRIORITY;
					}

					if(bank.wcb.mask)
					{
						MemoryRequest request;
						request.type = MemoryRequest::Type::STORE;
						request.paddr = bank.wcb.block_addr;
						request.size = _line_size;

						_mem_higher->request_bus.set_data(request, _mem_higher_request_index);
						_mem_higher->request_bus.set_pending(_mem_higher_request_index);

						bank.wcb.mask = 0x0ull;
						goto UPDATE_BANK_PRIORITY;
					}
				}
			}

		UPDATE_BANK_PRIORITY:
			_bank_priority_index = (_bank_priority_index + 1) % _banks.size();
		}

		void UnitCache::clock_rise()
		{
			_proccess_load_return();
			_proccess_requests();
		}

		void UnitCache::clock_fall()
		{
			if(_blocking) _update_banks_fall_blocking();
			else          _update_banks_fall();
		}
	}
}
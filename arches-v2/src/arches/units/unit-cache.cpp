#include "unit-cache.hpp"

namespace Arches {
	namespace Units {

		UnitCache::UnitCache(Configuration config, UnitMemoryBase* mem_higher, uint mem_higher_bank0_request_index, Simulator* simulator) : UnitMemoryBase(config.num_incoming_connections, simulator)
		{
			_configuration = config;

			_blocking = config.blocking;

			_cache_state.resize(config.cache_size / config.line_size);
			_banks.resize(config.num_banks, {config.num_incoming_connections, config.num_lse, config.num_mshr});

			_mem_higher = mem_higher;
			_mem_higher_bank0_request_index = mem_higher_bank0_request_index;
			
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
			_bank_index_offset = log2i(config.bank_stride * config.line_size);
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

		void UnitCache::_proccess_load_return()
		{
			for(uint bank_index = 0; bank_index < _banks.size(); ++bank_index)
			{
				uint mem_higher_return_index = _mem_higher_bank0_request_index + bank_index;
				if(_mem_higher->return_bus.get_pending(mem_higher_return_index))
				{
					const MemoryRequestItem& return_item = _mem_higher->return_bus.get_data(mem_higher_return_index);
					_mem_higher->return_bus.clear_pending(mem_higher_return_index);

					assert(return_item.type == MemoryRequestItem::Type::LOAD_RETURN);
					_insert_cache_line(return_item.line_paddr);

					_Bank& bank = _banks[bank_index];
					if(_blocking)
					{
						bank.block_for_load = false;
						bank.cycles_remaining = 1;
					}
					else
					{
						//get the miss status handling register and clear it then mark all the lses as ready to return
						uint mshr_index = bank.get_mshr(return_item.line_paddr);
						if(mshr_index != ~0u)
						{
							bank.mshrs[mshr_index].issued = false;
							bank.mshrs[mshr_index].valid = false;

							//then we need to copy the line and mark that load/store entry as ready to return
							for(uint lse_index = 0; lse_index < bank.lses.size(); ++lse_index)
							{
								if(bank.lses[lse_index].valid && bank.lses[lse_index].mshr_index == mshr_index)
								{
									assert(bank.lses[lse_index].request.line_paddr == bank.mshrs[mshr_index].line_paddr);
									bank.lses[lse_index].request.type = MemoryRequestItem::Type::LOAD_RETURN;
									bank.lses[lse_index].mshr_index = ~0u; //clear index since mshr in no longer valid
									bank.lses[lse_index].ready = true;

									//bank.return_arbitrator.push_request(lse_index);
								}
							}
						}
						else
						{
							//shouldn't happen
							printf("error\n");
							__debugbreak();
						}
					}
				}
			}
		}

		void UnitCache::_proccess_requests()
		{
			//loop till all entries incoming connection have been checked
			for(uint request_index = 0; request_index < _num_incoming_connections; ++request_index)
			{
				if(request_bus.get_pending(request_index))
				{
					const MemoryRequestItem& request_item = request_bus.get_data(request_index);
					uint32_t bank_index = _get_bank(request_item.line_paddr);
					_banks[bank_index].request_arbitrator.push_request(request_index);
				}
			}
		}

		void UnitCache::_update_banks_rise()
		{
			for(uint bank_index = 0; bank_index < _banks.size(); ++bank_index)
			{
				_Bank& bank = _banks[bank_index];

				if(!bank.active)
				{
					//if bank isn't busy and there are no pending requests to that bank then we can skip it
					bank.request_index = bank.request_arbitrator.pop_request();
					if(bank.request_index == ~0u) continue;

					bank.cycles_remaining = _penalty;
					bank.active = true;

					bank.current_request = request_bus.get_data(bank.request_index);
					request_bus.clear_pending(bank.request_index);
				}
			}
		}

		void UnitCache::_update_banks_fall_blocking()
		{
			for(uint bank_index = 0; bank_index < _banks.size(); ++bank_index)
			{
				_Bank& bank = _banks[bank_index];
				if(!bank.active) continue;
				if(bank.block_for_load) continue;

				uint mem_higher_request_index = _mem_higher_bank0_request_index + bank_index;
				if(bank.block_for_store)
				{
					//check if last request has been accepted
					if(!_mem_higher->request_bus.get_pending(mem_higher_request_index))
					{
						bank.block_for_store = false;
						bank.active = false;
					}
					continue;
				}

				if(bank.current_request.type == MemoryRequestItem::Type::STORE)
				{
					//forward store
					_mem_higher->request_bus.set_data(bank.current_request, mem_higher_request_index);
					_mem_higher->request_bus.set_pending(mem_higher_request_index);
					bank.block_for_store = true;
				}
				else if(--bank.cycles_remaining == 0) //run bank
				{
					//check if we have the data
					assert(_get_offset(bank.request.line_paddr) == 0);
					if(_get_cache_line(bank.current_request.line_paddr))
					{
						//if we do have the data return it to pending unit and release bank
						//check if last request has been accepted
						if(!return_bus.get_pending(bank.request_index))
						{
							log.log_hit();

							//std::memcpy(bank.request.data, line_data, _line_size);
							bank.current_request.type = MemoryRequestItem::Type::LOAD_RETURN;

							return_bus.set_data(bank.current_request, bank.request_index);
							return_bus.set_pending(bank.request_index);

							//free bank
							bank.active = false;
						}
						else
						{
							//stall
							bank.cycles_remaining = 1;
						}
					}
					else
					{
						//issue to mem_higher and stall
						if(!_mem_higher->request_bus.get_pending(mem_higher_request_index))
						{
							log.log_miss();
							_mem_higher->request_bus.set_data(bank.current_request, mem_higher_request_index);
							_mem_higher->request_bus.set_pending(mem_higher_request_index);
							bank.block_for_load = true;
						}
						else
						{
							bank.cycles_remaining = 1;
						}
					}
				}
			}
		}

		void UnitCache::_update_banks_fall()
		{
			for(uint bank_index = 0; bank_index < _banks.size(); ++bank_index)
			{
				_Bank& bank = _banks[bank_index];
				if(!bank.active) continue;

				if(bank.current_request.type == MemoryRequestItem::Type::STORE)
				{
					//stores go around the cache so we just need to add an lse
					uint lse_index = bank.get_free_lse();
					if(lse_index != ~0u)
					{
						bank.lses[lse_index].mshr_index = ~0u; //wasn't a miss
						bank.lses[lse_index].request_index = ~0u; //don't need
						bank.lses[lse_index].request = bank.current_request;
						bank.lses[lse_index].valid = true;
						bank.lses[lse_index].ready = true;
						//bank.return_arbitrator.push_request(lse_index);

						//free bank
						bank.active = false;
					}
					else //stall since lse is full (try again next cycle)
					{
						log.log_lse_stall();
					}
				}
				else if(bank.current_request.type == MemoryRequestItem::Type::LOAD)
				{
					if(bank.cycles_remaining > 0) bank.cycles_remaining--;
					if(bank.cycles_remaining == 0) //run bank
					{
						//check if we have the data
						assert(_get_offset(bank.current_request.line_paddr) == 0);
						if(_get_cache_line(bank.current_request.line_paddr))
						{
							//if we do have the data return it to pending unit and release bank
							uint lse_index = bank.get_free_lse();
							if(lse_index != ~0u)
							{
								bank.current_request.type = MemoryRequestItem::Type::LOAD_RETURN;

								bank.lses[lse_index].mshr_index = ~0u; //wasn't a miss
								bank.lses[lse_index].request_index = bank.request_index;
								bank.lses[lse_index].request = bank.current_request;
								bank.lses[lse_index].valid = true;
								bank.lses[lse_index].ready = true;
								//bank.return_arbitrator.push_request(lse_index);

								//free bank
								bank.active = false;
								log.log_hit();
							}
							else //stall since lse is full(try again next cycle)
							{
								log.log_lse_stall();
							}
						}
						else
						{
							//if we dont have the data try to update mshr and lse if we can't then we stall (aka block)
							uint mshr_index = bank.get_mshr(bank.current_request.line_paddr);
							if(mshr_index == ~0u) mshr_index = bank.get_free_mshr();
							uint lse_index = bank.get_free_lse();

							if(lse_index != ~0u && mshr_index != ~0u)
							{

								if(!bank.mshrs[mshr_index].valid)
								{
									bank.mshrs[mshr_index].line_paddr = bank.current_request.line_paddr;
									bank.mshrs[mshr_index].valid = true;
									bank.mshrs[mshr_index].issued = false;
								}

								bank.lses[lse_index].mshr_index = mshr_index;
								bank.lses[lse_index].request_index = bank.request_index;
								bank.lses[lse_index].request = bank.current_request;
								bank.lses[lse_index].valid = true;
								bank.lses[lse_index].ready = false;

								//free bank
								bank.active = false;
								log.log_miss();
							}
							else //stall since lse or mshr is full(try again next cycle)
							{
								if(lse_index == ~0u) log.log_lse_stall();
								if(mshr_index == ~0u) log.log_mshr_stall();
							}
						}
					}
				}
			}

			for(uint bank_index = 0; bank_index < _banks.size(); ++bank_index)
			{
				_Bank& bank = _banks[bank_index];
				uint mem_higher_request_index = _mem_higher_bank0_request_index + bank_index;

				//try to issue next ls entry
				for(uint lse_index = 0; lse_index < bank.lses.size(); ++lse_index)
				{
					if(bank.lses[lse_index].ready)
					{
						//stores have higher priority than loads
						if(bank.lses[lse_index].request.type == MemoryRequestItem::Type::STORE) //store
						{
							if(!_mem_higher->request_bus.get_pending(mem_higher_request_index))
							{
								_mem_higher->request_bus.set_data(bank.lses[lse_index].request, mem_higher_request_index);
								_mem_higher->request_bus.set_pending(mem_higher_request_index);

								bank.lses[lse_index].ready = false;
								bank.lses[lse_index].valid = false;
							}
						}
						else if(bank.lses[lse_index].request.type == MemoryRequestItem::Type::LOAD_RETURN) //load_return
						{
							//check if last return has been accepted
							uint request_index = bank.lses[lse_index].request_index;
							if(!return_bus.get_pending(request_index))
							{
								return_bus.set_data(bank.lses[lse_index].request, request_index);
								return_bus.set_pending(request_index);

								bank.lses[lse_index].ready = false;
								bank.lses[lse_index].valid = false;
							}
						}
					}
				}

				//try to issue next miss
				if(!_mem_higher->request_bus.get_pending(mem_higher_request_index))
				{
					for(uint mshr_index = 0; mshr_index < bank.mshrs.size(); ++mshr_index)
					{
						if(bank.mshrs[mshr_index].valid && !bank.mshrs[mshr_index].issued)
						{
							MemoryRequestItem request;
							request.size = _line_size;
							request.line_paddr = bank.mshrs[mshr_index].line_paddr;
							request.type = MemoryRequestItem::Type::LOAD;

							_mem_higher->request_bus.set_data(request, mem_higher_request_index);
							_mem_higher->request_bus.set_pending(mem_higher_request_index);

							bank.mshrs[mshr_index].issued = true;

							break;
						}
					}
				}
			}
		}

		void UnitCache::clock_rise()
		{
			_proccess_load_return();
			_proccess_requests();
			_update_banks_rise();
		}

		void UnitCache::clock_fall()
		{
			if(_blocking) _update_banks_fall_blocking();
			else         _update_banks_fall();
		}
	}
}
#include "unit-non-blocking-cache.hpp"

namespace Arches {
	namespace Units {

		UnitNonBlockingCache::UnitNonBlockingCache(Configuration config, UnitMemoryBase* mem_higher, uint mem_higher_bus_start_index, Simulator* simulator) : UnitMemoryBase(config.num_incoming_ports, simulator)
		{
			_configuration = config;

			_data_u8.resize(config.cache_size);
			_cache_state.resize(config.cache_size / config.line_size);
			_banks.resize(config.num_banks, config.num_incoming_ports);
			_port_locked.resize(config.num_incoming_ports);

			_mem_higher = mem_higher;
			_mem_higher_ports_start_index = mem_higher_bus_start_index;
			
			_num_incoming_ports = config.num_incoming_ports;

			_associativity = config.associativity;
			_line_size = config.line_size;
			_penalty = config.penalty;

			uint num_sets = config.cache_size / (config.line_size * config.associativity);

			offset_bits = log2i(config.line_size);
			_set_index_bits = log2i(num_sets);
			_tag_bits = static_cast<uint>(sizeof(paddr_t) * 8) - (_set_index_bits + offset_bits);
			_bank_index_bits = log2i(config.num_banks);

			offset_mask = generate_nbit_mask(offset_bits);
			_set_index_mask = generate_nbit_mask(_set_index_bits);
			_tag_mask = generate_nbit_mask(_tag_bits);
			_bank_index_mask = generate_nbit_mask(_bank_index_bits);

			_set_index_offset = offset_bits;
			_tag_offset = offset_bits + _set_index_bits;
			_bank_index_offset = log2i(config.bank_stride * config.line_size);
		}

		UnitNonBlockingCache::~UnitNonBlockingCache()
		{
			//delete[] _data_u8;
			//delete[] _cache_state;
			//delete[] _ports;
			//delete[] _banks;
		}

		//update lru and returns data pointer to cache line
		uint8_t* UnitNonBlockingCache::_get_cache_line(paddr_t paddr)
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

			if(found_index == ~0) return nullptr; //didn't find line so we will leave lru alone and return nullptr

			for(uint32_t i = start; i < end; ++i)
				if(_cache_state[i].lru < found_lru) _cache_state[i].lru++;

			_cache_state[found_index].lru = 0;

			return _data_u8.data() + static_cast<size_t>(found_index << offset_bits);
		}

		//inserts cacheline associated with paddr replacing least recently used. Assumes cachline isn't already in cache if it is this has undefined behaviour
		void UnitNonBlockingCache::_insert_cache_line(paddr_t paddr, const uint8_t* data)
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

			uint8_t* dst = _data_u8.data() + static_cast<size_t>(replacement_index << offset_bits);
			std::memcpy(dst, data, _line_size);
		}

		void UnitNonBlockingCache::_proccess_load_return()
		{
			for(uint bank_index = 0; bank_index < _banks.size(); ++bank_index)
			{
				uint mem_higher_return_port_index = _mem_higher_ports_start_index + bank_index;
				if(_mem_higher->return_bus.get_pending(mem_higher_return_port_index))
				{
					const MemoryRequestItem& return_item = _mem_higher->return_bus.get_bus_data(mem_higher_return_port_index);
					_mem_higher->return_bus.clear_pending(mem_higher_return_port_index);

					assert(return_item.type == MemoryRequestItem::Type::LOAD_RETURN);
					_insert_cache_line(return_item.line_paddr, return_item.data);

					//uint bank_index = _get_bank(return_item.line_paddr);
					_Bank& bank = _banks[bank_index];

					//get the miss status handling register and clear it then mark all the lses as ready to return
					uint mshr_index = bank.get_mshr(return_item.line_paddr);
					if(mshr_index != ~0u)
					{
						bank.mshrs[mshr_index].issued = false;
						bank.mshrs[mshr_index].valid = false;

						//then we need to copy the line and mark that load/store entry as ready to return
						for(uint lse_index = 0; lse_index < NUM_LSE; ++lse_index)
						{
							if(bank.lses[lse_index].valid && bank.lses[lse_index].mshr_index == mshr_index)
							{
								assert(bank.lses[lse_index].request.line_paddr == bank.mshrs[mshr_index].line_paddr);
								std::memcpy(bank.lses[lse_index].request.data, return_item.data, CACHE_LINE_SIZE);
								bank.lses[lse_index].request.type = MemoryRequestItem::Type::LOAD_RETURN;
								bank.lses[lse_index].mshr_index = ~0u; //clear index since mshr in no longer valid
								bank.lses[lse_index].ready = true;
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

		void UnitNonBlockingCache::_proccess_requests()
		{
			//loop till all entries incoming connection have been checked
			for(uint port_index = 0; port_index < _num_incoming_ports; ++port_index)
			{
				if(request_bus.get_pending(port_index))
				{
					const MemoryRequestItem& request_item = request_bus.get_bus_data(port_index);
					//assert(request_item.type != MemoryRequestItem::Type::STORE);
					//if(request_item.type == MemoryRequestItem::Type::LOAD)
					{
						uint32_t bank_index = _get_bank(request_item.line_paddr);
						_banks[bank_index].arbitrator.push_request(port_index);
					}
				}
			}
		}

		void UnitNonBlockingCache::_update_banks_rise()
		{
			for(uint bank_index = 0; bank_index < _banks.size(); ++bank_index)
			{
				_Bank& bank = _banks[bank_index];

				if(!bank.active)
				{
					//if bank isn't busy and there are no pending requests to that bank then we can skip it
					bank.port_index = bank.arbitrator.pop_request();
					if(bank.port_index == ~0u) continue;

					bank.cycles_remaining = _penalty;
					bank.active = true;

					bank.request = request_bus.get_bus_data(bank.port_index);
					request_bus.clear_pending(bank.port_index);
				}
			}
		}

		void UnitNonBlockingCache::_update_banks_fall()
		{
			for(uint bank_index = 0; bank_index < _banks.size(); ++bank_index)
			{
				_Bank& bank = _banks[bank_index];
				if(!bank.active) continue;

				uint mem_higher_request_port_index = _mem_higher_ports_start_index + bank_index;
				if(bank.request.type == MemoryRequestItem::Type::STORE)
				{
					//stores go around the cache so we just need to add an lse
					uint lse_index = bank.get_free_lse();
					if(lse_index != ~0u)
					{
						bank.lses[lse_index].mshr_index = ~0u; //wasn't a miss
						bank.lses[lse_index].port_index = ~0u; //don't need
						bank.lses[lse_index].request = bank.request;
						bank.lses[lse_index].valid = true;
						bank.lses[lse_index].ready = true;

						//free bank
						bank.active = false;
					}
					//else stall since lse is full (try again next cycle)
				}
				else if(bank.request.type == MemoryRequestItem::Type::LOAD)
				{
					if(bank.cycles_remaining > 0) bank.cycles_remaining--;
					if(bank.cycles_remaining == 0) //run bank
					{
						//check if we have the data
						assert(_get_offset(bank.request.line_paddr) == 0);
						uint8_t* line_data = _get_cache_line(bank.request.line_paddr);
						if(line_data)
						{
							//if we do have the data return it to pending unit and release bank
							uint lse_index = bank.get_free_lse();
							if(lse_index != ~0u)
							{
								std::memcpy(bank.request.data, line_data, _line_size);
								bank.request.type = MemoryRequestItem::Type::LOAD_RETURN;

								bank.lses[lse_index].mshr_index = ~0u; //wasn't a miss
								bank.lses[lse_index].port_index = bank.port_index;
								bank.lses[lse_index].request = bank.request;
								bank.lses[lse_index].valid = true;
								bank.lses[lse_index].ready = true;

								//free bank
								bank.active = false;
								log.log_hit();
							}
							//else stall since lse is full (try again next cycle)
						}
						else
						{
							//if we dont have the data try to update mshr and lse if we can't then we stall (aka block)
							uint mshr_index = bank.get_mshr(bank.request.line_paddr);
							if(mshr_index == ~0u) mshr_index = bank.get_free_mshr();
							uint lse_index = bank.get_free_lse();

							if(lse_index != ~0u && mshr_index != ~0u)
							{

								if(!bank.mshrs[mshr_index].valid)
								{
									bank.mshrs[mshr_index].line_paddr = bank.request.line_paddr;
									bank.mshrs[mshr_index].valid = true;
									bank.mshrs[mshr_index].issued = false;
								}

								bank.lses[lse_index].mshr_index = mshr_index;
								bank.lses[lse_index].port_index = bank.port_index;
								bank.lses[lse_index].request = bank.request;
								bank.lses[lse_index].valid = true;
								bank.lses[lse_index].ready = false;

								//free bank
								bank.active = false;
								log.log_miss();
							}
							//else stall since lse or mshr is full (try again next cycle)
						}
					}
				}
			}

			for(uint bank_index = 0; bank_index < _banks.size(); ++bank_index)
			{
				_Bank& bank = _banks[bank_index];
				uint mem_higher_request_port_index = _mem_higher_ports_start_index + bank_index;

				//try to issue next ls entry
				for(uint lse_index = 0; lse_index < NUM_LSE; ++lse_index)
				{
					if(bank.lses[lse_index].ready)
					{
						//stores have higher priority than loads
						if(bank.lses[lse_index].request.type == MemoryRequestItem::Type::STORE) //store
						{
							if(!_mem_higher->request_bus.get_pending(mem_higher_request_port_index))
							{
								_mem_higher->request_bus.set_bus_data(bank.lses[lse_index].request, mem_higher_request_port_index);
								_mem_higher->request_bus.set_pending(mem_higher_request_port_index);

								bank.lses[lse_index].ready = false;
								bank.lses[lse_index].valid = false;
							}
						}
						else if(bank.lses[lse_index].request.type == MemoryRequestItem::Type::LOAD_RETURN) //load_return
						{
							//check if last return has been accepted
							uint port_index = bank.lses[lse_index].port_index;
							if(!return_bus.get_pending(port_index))
							{
								return_bus.set_bus_data(bank.lses[lse_index].request, port_index);
								return_bus.set_pending(port_index);

								bank.lses[lse_index].ready = false;
								bank.lses[lse_index].valid = false;
							}
						}
					}
				}

				//try to issue next miss
				if(!_mem_higher->request_bus.get_pending(mem_higher_request_port_index))
				{
					for(uint mshr_index = 0; mshr_index < NUM_MSHR; ++mshr_index)
					{
						if(bank.mshrs[mshr_index].valid && !bank.mshrs[mshr_index].issued)
						{
							MemoryRequestItem request;
							request.size = CACHE_LINE_SIZE;
							request.line_paddr = bank.mshrs[mshr_index].line_paddr;
							request.type = MemoryRequestItem::Type::LOAD;

							_mem_higher->request_bus.set_bus_data(request, mem_higher_request_port_index);
							_mem_higher->request_bus.set_pending(mem_higher_request_port_index);

							bank.mshrs[mshr_index].issued = true;

							break;
						}
					}
				}
			}
		}

		void UnitNonBlockingCache::clock_rise()
		{
			_proccess_load_return();
			_proccess_requests();
			_update_banks_rise();
		}

		void UnitNonBlockingCache::clock_fall()
		{
			_update_banks_fall();
		}
	}
}
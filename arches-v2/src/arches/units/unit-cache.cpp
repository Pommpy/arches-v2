#include "unit-cache.hpp"

namespace Arches {
	namespace Units {

		UnitCache::UnitCache(CacheConfiguration config, UnitMemoryBase* mem_higher, Simulator* simulator) : UnitMemoryBase(mem_higher, simulator)
		{
			_configuration = config;

			_data_u8.resize(config.cache_size);
			_cache_state.resize(config.cache_size / config.line_size);
			_banks.resize(config.num_banks);

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

			output_buffer.resize(static_cast<uint>(_banks.size()) * 4u, static_cast<uint>(sizeof(MemoryRequestItem))); //TODO tighter bounds
			acknowledge_buffer.resize(static_cast<uint>(_banks.size()));//every bank can acknowledge a request on a given cycle
		}

		UnitCache::~UnitCache()
		{
			//delete[] _data_u8;
			//delete[] _cache_state;
			//delete[] _ports;
			//delete[] _banks;
		}

		//update lru and returns data pointer to cache line
		uint8_t* UnitCache::_get_cache_line(paddr_t paddr)
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
		void UnitCache::_insert_cache_line(paddr_t paddr, uint8_t* data)
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

		void UnitCache::_proccess_load_return()
		{
			_return_buffer.reset_arbitrator_lowest_index();
			uint return_index = _return_buffer.get_next_index();
			if(return_index != ~0)
			{
				MemoryRequestItem* return_item = _return_buffer.get_message(return_index);
				assert(return_item->type == MemoryRequestItem::Type::LOAD_RETURN);
				_insert_cache_line(return_item->paddr, return_item->data);
				_banks[_get_bank(return_item->paddr)].busy = false; //free bank to accept next request
				_return_buffer.clear(return_index);
				_pending_load_return = false;
			}
		}

		void UnitCache::_proccess_requests()
		{
			uint request_index = 0;
			_request_buffer.reset_arbitrator_lowest_index();
			while ((request_index = _request_buffer.get_next_index()) != ~0) //loop till all entries incoming connection have been checked
			{
				MemoryRequestItem* request_item = _request_buffer.get_message(request_index);
				assert(request_item->type != MemoryRequestItem::Type::STORE);
				if (request_item->type == MemoryRequestItem::Type::LOAD)
				{
					paddr_t aligned_paddr = _get_cache_line_start_paddr(request_item->paddr);
					uint32_t bank = _get_bank(aligned_paddr);
					if (_banks[bank].busy) continue; //we can't accept this request on this cycle since the bank is busy so we go on to the next

					uint32_t request_offest = (_request_buffer.size() + request_index - _banks[bank].arbitrator_index) % _request_buffer.size();
					if (request_offest < _banks[bank].candidate_request_offset) _banks[bank].candidate_request_offset = request_offest;
				}
			}
		}

		void UnitCache::_update_banks()
		{
			for (uint bank = 0; bank < _banks.size(); ++bank)
			{
				//if we are waiting for data or dont have a bank then do nothing
				if (!_banks[bank].busy)
				{
					//if bank isn't busy and there are no pending requests to that bank then we can skip it
					if (_banks[bank].candidate_request_offset == ~0) continue;

					uint request_index = (_banks[bank].arbitrator_index + _banks[bank].candidate_request_offset) % _request_buffer.size();

					_banks[bank].request = *_request_buffer.get_message(request_index);
					acknowledge_buffer.push_message(_request_buffer.get_sending_unit(request_index), _request_buffer.id);
					_request_buffer.clear(request_index);

					_banks[bank].cycles_remaining = _penalty;
					_banks[bank].sent_load = false;
					_banks[bank].miss = false;
					_banks[bank].busy = true;

					_banks[bank].arbitrator_index = (request_index + 1) % _request_buffer.size();
					_banks[bank].candidate_request_offset = ~0;
				}

				if (!_banks[bank].miss && (--_banks[bank].cycles_remaining == 0))
				{
					//check if we have the data
					MemoryRequestItem& request = _banks[bank].request;
					assert(_get_offset(request.paddr) == 0);
					uint8_t* line_data = _get_cache_line(_banks[bank].request.paddr);
					if (line_data)
					{
						//if we do return data to pending unit and release bank
						log.log_hit();

						request.type = MemoryRequestItem::Type::LOAD_RETURN;
						std::memcpy(request.data, line_data, _line_size);

						for (uint8_t i = 0; i < request.return_buffer_id_stack_size; ++i)
							output_buffer.push_message(&request, request.return_buffer_id_stack[i], 0);

						_banks[bank].busy = false;
					}
					else
					{
						//if we dont issue load to mem higher
						log.log_miss();

						request.return_buffer_id_stack[request.return_buffer_id_stack_size++] = _return_buffer.id;
						_banks[bank].miss = true;
					}
				}
			}
		}
		
		void UnitCache::_try_issue_next_load()
		{
			if(_pending_load_return) return;
			for(uint bank = 0; bank < _banks.size(); ++bank)
			{
				if(_banks[bank].miss && !_banks[bank].sent_load)
				{
					uint _bank_offset = (static_cast<uint>(_banks.size()) + bank - _bank_request_arbitrator_index) % _banks.size();
					if(_bank_offset < _next_bank_requesting_offset) _next_bank_requesting_offset = _bank_offset;
				}
			}

			if(_next_bank_requesting_offset == ~0u) return;
			
			_bank_request_arbitrator_index = (_bank_request_arbitrator_index + _next_bank_requesting_offset) % _banks.size();

			MemoryRequestItem& request = _banks[_bank_request_arbitrator_index].request;
			_banks[_bank_request_arbitrator_index].sent_load = true;
			output_buffer.push_message(&request, _memory_higher_buffer_id, _client_id);
			_pending_load_return = true;
			
			_bank_request_arbitrator_index = (_bank_request_arbitrator_index + 1) % _banks.size();
			_next_bank_requesting_offset = ~0u;
		}

		void UnitCache::acknowledge(buffer_id_t buffer)
		{
			assert(buffer == _memory_higher_buffer_id);
			//_pending_load_return = false;
			//_try_issue_next_load();
		}

		void UnitCache::execute()
		{
			_proccess_load_return();

			_proccess_requests();

			_update_banks();

			_try_issue_next_load();
		}
	}
}
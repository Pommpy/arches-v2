#include "unit-cache.hpp"

namespace Arches {
	namespace Units {

		UnitCache::UnitCache(CacheConfiguration config, UnitMemoryBase* mem_higher, uint32_t num_clients, Simulator* simulator) : UnitMemoryBase(mem_higher, num_clients, simulator)
		{
			_configuration = config;

			_data_u8.resize(config.cache_size);
			_cache_state.resize(config.cache_size / config.line_size);
			_banks.resize(config.num_banks);

			_associativity = config.associativity;
			_line_size = config.line_size;
			_penalty = config.penalty;

			uint num_sets = config.cache_size / (config.line_size * config.associativity);

			_offset_bits = log2i(config.line_size);
			_set_index_bits = log2i(num_sets);
			_tag_bits = static_cast<uint>(sizeof(physical_address) * 8) - (_set_index_bits + _offset_bits);
			_bank_index_bits = log2i(config.num_banks);

			_offset_mask = generate_nbit_mask(_offset_bits);
			_set_index_mask = generate_nbit_mask(_set_index_bits);
			_tag_mask = generate_nbit_mask(_tag_bits);
			_bank_index_mask = generate_nbit_mask(_bank_index_bits);

			_set_index_offset = _offset_bits;
			_tag_offset = _offset_bits + _set_index_bits;
			_bank_index_offset = log2i(config.bank_stride * config.line_size);

			output_buffer.resize(_banks.size(), sizeof(MemoryRequestItem));
		}

		UnitCache::~UnitCache()
		{
			//delete[] _data_u8;
			//delete[] _cache_state;
			//delete[] _ports;
			//delete[] _banks;
		}

		//update lru and returns data pointer to cache line
		uint8_t* UnitCache::_get_cache_line(physical_address paddr)
		{
			uint64_t start = _get_set_index(paddr) * _associativity;
			uint64_t end = start + _associativity;

			uint64_t tag = _get_tag(paddr);

			uint found_index = ~0;
			uint found_lru = 0;
			for(uint i = start; i < end; ++i)
			{
				if(_cache_state[i].valid && _cache_state[i].tag == tag)
				{
					found_index = i;
					found_lru = _cache_state[i].lru;
					break;
				}
			}

			if(found_index == ~0) return nullptr; //didn't find line so we will eave lru alone and return nullptr

			for(uint i = start; i < end; ++i)
				if(_cache_state[i].lru < found_lru) _cache_state[i].lru++;

			_cache_state[found_index].lru = 0;

			return _get_cache_line_data_pointer(found_index);
		}

		//inserts cacheline associated with paddr replacing least recently used. Assumes cachline isn't already in cache if it is this has undefined behaviour
		void UnitCache::_insert_cache_line(physical_address paddr, uint8_t* data)
		{
			uint64_t start = _get_set_index(paddr) * _associativity;
			uint64_t end = start + _associativity;

			uint replacement_index = ~0;
			uint replacement_lru = 0;
			for(uint i = start; i < end; ++i)
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

			for(uint i = start; i < end; ++i)
				_cache_state[i].lru++;

			_cache_state[replacement_index].lru = 0;
			_cache_state[replacement_index].valid = true;
			_cache_state[replacement_index].tag = _get_tag(paddr);

			uint8_t* dst = _get_cache_line_data_pointer(replacement_index);
			std::memcpy(dst, data, _line_size);
		}

		void UnitCache::_try_insert_request(physical_address paddr, uint index)
		{
			physical_address aligned_paddr = _get_cache_line_start_paddr(paddr);
			uint bank = _get_bank(aligned_paddr);
			if(_banks[bank].busy) return; //we can't accept this request on this cycle since the bank is busy so we go on to the next

			uint request_offest = (index + _request_buffer.size() - _banks[bank].arbitrator_index) % _request_buffer.size();
			if(_banks[bank].candidate_request_index == ~0) _banks[bank].candidate_request_index = index;
			else
			{
				uint candidate_request_offset = (_banks[bank].candidate_request_index + _request_buffer.size() - _banks[bank].arbitrator_index) % _request_buffer.size();
				if(request_offest < candidate_request_offset) _banks[bank].candidate_request_index = index;
			}
		}

		void UnitCache::_update_bank(uint bank)
		{
			//if we are waiting for data or dont have a bank then do nothing
			if(!_banks[bank].busy)
			{
				if(_banks[bank].candidate_request_index == ~0) return;

				uint index = _banks[bank].candidate_request_index;
				_banks[bank].request = *_request_buffer.get(index);
				_request_buffer.clear_index(index);

				_banks[bank].cycles_remaining = _penalty;
				_banks[bank].pending_mem_higher = false;
				_banks[bank].busy = true;
			
				_banks[bank].arbitrator_index = (index + 1) % _request_buffer.size();
				_banks[bank].candidate_request_index = ~0;
			} 
			else if(_banks[bank].pending_mem_higher) return;

			if(_banks[bank].cycles_remaining == 1) 
			{
				//check if we have the data
				//if we dont issue load to mem higher
				//if we do return data to pendign unit and release port
			}
			else _banks[bank].cycles_remaining--;
		}

		void UnitCache::execute()
		{
			MemoryRequestItem* request_item; uint request_index = 0;
			_request_buffer.rest_arbitrator_lowest_index();

			//loop till all entries incoming connection have been checked
			while((request_item = _request_buffer.get_next(request_index)) != nullptr)
			{
				assert(request_item->type != MemoryRequestItem::STORE);
				if(request_item->type == MemoryRequestItem::LOAD)
					_try_insert_request(request_item->paddr, request_index);

				if(request_item->type == MemoryRequestItem::LOAD_RETURN);
				{
					//update pending bank
				}
			}

			for(uint i = 0; i < _banks.size(); ++i)
				_update_bank(i);
		}
	}
}
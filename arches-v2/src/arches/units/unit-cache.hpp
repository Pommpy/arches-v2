#pragma once 
#include "../../stdafx.hpp"

#include "../util/bit-manipulation.hpp"
#include "unit-base.hpp"
#include "unit-memory-base.hpp"

namespace Arches {
	namespace Units {

		class UnitCache final : public UnitMemoryBase
		{
		public:
			struct CacheConfiguration
			{
				uint cache_size{1024};
				uint line_size{32};
				uint associativity{1};
				uint num_banks{1};
				uint bank_stride{1};
				uint penalty{1};
			};

		private:
			struct _CacheLine
			{
				uint64_t tag;
				uint8_t lru;
				bool valid;
			};

			struct _Bank
			{
				MemoryRequestItem request;
				uint cycles_remaining{0};
				bool pending_mem_higher{false};
				bool busy{false};

				uint arbitrator_index{0};
				uint candidate_request_index{0};
			};

			CacheConfiguration _configuration;

			uint  _penalty;
			uint _offset_bits, _set_index_bits, _tag_bits, _bank_index_bits;
			uint64_t _offset_mask, _set_index_mask, _tag_mask, _bank_index_mask;
			uint _set_index_offset, _tag_offset, _bank_index_offset;

			uint _associativity, _line_size;
			std::vector<_CacheLine> _cache_state;
			std::vector<uint8_t> _data_u8; //backing data left seperate to reduce stride when updating lru

			std::vector<_Bank> _banks;

			bool _waiting_on_mem_higher_to_accept_request{false};
			uint _port_to_release_on_ready{~0};


		public:
			UnitCache(CacheConfiguration config, UnitMemoryBase* mem_higher, uint32_t num_clients, Simulator* simulator);
			virtual ~UnitCache();
			void execute() override;

		private:
			void _update_bank(uint bank);

			uint64_t _get_offset(physical_address paddr) { return (paddr >> 0) & _offset_mask; }
			uint64_t _get_set_index(physical_address paddr) { return (paddr >> _set_index_offset) & _set_index_mask; }
			uint64_t _get_tag(physical_address paddr) { return (paddr >> _tag_offset) & _tag_mask; }
			uint64_t _get_bank(physical_address paddr) { return (paddr >> _bank_index_offset) & _bank_index_mask; }

			uint64_t _get_cache_line_start_paddr(physical_address paddr) { return (paddr >> _set_index_offset) << _set_index_offset; }

			uint8_t* _get_cache_line_data_pointer(uint cache_index)
			{
				uint data_index = cache_index;
				data_index <<= _offset_bits;
				return _data_u8.data() + data_index;
			}

			uint8_t* _get_cache_line(physical_address paddr);
			void _insert_cache_line(physical_address paddr, uint8_t* data);

			void _try_insert_request(physical_address paddr, uint index);
		};


	}
}
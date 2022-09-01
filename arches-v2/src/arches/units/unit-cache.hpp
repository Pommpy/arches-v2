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

			class Log
			{
			private:
				uint64_t _hits;
				uint64_t _misses;

			public:
				Log() { reset(); }

				void reset()
				{
					_hits = 0;
					_misses = 0;
				}


				void accumulate(const Log& other)
				{
						_hits += other._hits;
						_misses += other._misses;
				}

				void log_hit()
				{
					_hits++;
				}

				void log_miss()
				{
					_misses++;
				}

				void print_log()
				{
					printf("Total: %lld\n", _misses + _hits);
					printf("Hits: %lld\n", _hits);
					printf("Misses: %lld\n", _misses);
				}
			}log;

		private:
			struct _CacheLine
			{
				uint64_t tag{~0ull};
				uint8_t lru{0u};
				bool valid{false};
			};

			struct _Bank
			{
				MemoryRequestItem request;
				uint cycles_remaining{0};
				bool sent_load{false};
				bool miss{false};
				bool busy{false};

				uint arbitrator_index{~0u};
				uint candidate_request_offset{~0u};
			};

			CacheConfiguration _configuration;

			uint  _penalty;
			uint _set_index_bits, _tag_bits, _bank_index_bits;
			uint64_t _set_index_mask, _tag_mask, _bank_index_mask;
			uint _set_index_offset, _tag_offset, _bank_index_offset;

			uint _associativity, _line_size;
			std::vector<_CacheLine> _cache_state;
			std::vector<uint8_t> _data_u8; //backing data left seperate to reduce stride when updating lru

			std::vector<_Bank> _banks;

			bool _pending_load_return{false};
			uint _bank_request_arbitrator_index{0};
			uint _next_bank_requesting_offset{~0u};
			
		public:
			UnitCache(CacheConfiguration config, UnitMemoryBase* mem_higher, Simulator* simulator);
			virtual ~UnitCache();
			void execute() override;
			void acknowledge(buffer_id_t buffer) override;

		private:
			void _proccess_load_return();
			void _proccess_requests();
			void _update_banks();
			void _try_issue_next_load();

			uint8_t* _get_cache_line(paddr_t paddr);
			void _insert_cache_line(paddr_t paddr, uint8_t* data);

			uint32_t _get_offset(paddr_t paddr) { return  static_cast<uint32_t>((paddr >> 0) & offset_mask); }
			uint32_t _get_set_index(paddr_t paddr) { return  static_cast<uint32_t>((paddr >> _set_index_offset) & _set_index_mask); }
			uint64_t _get_tag(paddr_t paddr) { return (paddr >> _tag_offset) & _tag_mask; }
			
			uint32_t _get_bank(paddr_t paddr) { return static_cast<uint32_t>((paddr >> _bank_index_offset) & _bank_index_mask); }

			paddr_t _get_cache_line_start_paddr(paddr_t paddr) { return paddr & ~offset_mask; }
		};
	}
}
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
			struct Configuration
			{
				uint cache_size{1024};
				uint line_size{32};
				uint associativity{1};
				uint num_banks{1};
				uint num_incoming_ports{1};
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

				void print_log(uint units = 1)
				{
					uint64_t total = _misses + _hits;
					float hit_rate = static_cast<float>(_hits) / total;
					float miss_rate = static_cast<float>(_misses) / total;

					//printf("Overall\n");
					//printf("Total: %lld\n", total);
					//printf("Hits: %lld\n", _hits);
					//printf("Misses: %lld\n", _misses);
					//printf("Hit Rate: %.2f%%\n", hit_rate * 100.0f);
					//printf("Miss Rate: %.2f%%\n", miss_rate * 100.0f);

					printf("Total: %lld\n", total / units);
					printf("Hits: %lld\n", _hits / units);
					printf("Misses: %lld\n", _misses / units);
					printf("Hit Rate: %.2f%%\n", hit_rate * 100.0f);
					printf("Miss Rate: %.2f%%\n", miss_rate * 100.0f);
				}
			}log;

		private:
			struct _CacheLine
			{
				uint64_t tag     : 59;
				uint64_t lru     : 4;
				uint64_t valid   : 1;
			};

			struct _Bank
			{
				RoundRobinArbitrator arbitrator;

				MemoryRequestItem request; //current request
				uint cycles_remaining{0u};
				uint port_index{~0u};
				bool active{false};
				bool block_for_store{false};
				bool block_for_load{false};

				_Bank(uint num_ports) : arbitrator(num_ports) {}
			};

			Configuration _configuration;

			uint  _penalty;
			uint _set_index_bits, _tag_bits, _bank_index_bits;
			uint64_t _set_index_mask, _tag_mask, _bank_index_mask;
			uint _set_index_offset, _tag_offset, _bank_index_offset;

			uint _associativity, _line_size;
			std::vector<_CacheLine> _cache_state;
			std::vector<uint8_t> _data_u8; //backing data left seperate to reduce stride when updating lru

			std::vector<_Bank> _banks;
			uint _num_incoming_ports;

			UnitMemoryBase* _mem_higher;
			uint _mem_higher_ports_start_index;

		public:
			UnitCache(Configuration config, UnitMemoryBase* mem_higher, uint bus_index_start, Simulator* simulator);
			virtual ~UnitCache();
			void clock_rise() override;
			void clock_fall() override;

		private:
			void _proccess_load_return();
			void _proccess_requests();
			void _update_banks_rise();
			void _update_banks_fall();

			uint8_t* _get_cache_line(paddr_t paddr);
			void _insert_cache_line(paddr_t paddr, const uint8_t* data);

			uint32_t _get_offset(paddr_t paddr) { return  static_cast<uint32_t>((paddr >> 0) & offset_mask); }
			uint32_t _get_set_index(paddr_t paddr) { return  static_cast<uint32_t>((paddr >> _set_index_offset) & _set_index_mask); }
			uint64_t _get_tag(paddr_t paddr) { return (paddr >> _tag_offset) & _tag_mask; }
			
			uint32_t _get_bank(paddr_t paddr) { return static_cast<uint32_t>((paddr >> _bank_index_offset) & _bank_index_mask); }

			paddr_t _get_cache_line_start_paddr(paddr_t paddr) { return paddr & ~offset_mask; }
		};
	}
}
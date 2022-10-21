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
				uint line_size{CACHE_LINE_SIZE};
				uint associativity{1};
				uint num_banks{1};
				uint num_incoming_connections{1};
				uint bank_stride{1};
				bool blocking{false};
				uint penalty{1};
				uint num_lse{1};
				uint num_mshr{1};
			};

			class Log
			{
			private:
				uint64_t _hits;
				uint64_t _hit_under_misses;
				uint64_t _misses;

				uint64_t _lse_stalls;
				uint64_t _mshr_stalls;

				uint64_t _bank_conflicts;

			public:
				Log() { reset(); }

				void reset()
				{
					_hits = 0;
					_misses = 0;
					_hit_under_misses = 0;
					_lse_stalls = 0;
					_mshr_stalls = 0;
					_bank_conflicts = 0;
				}

				void accumulate(const Log& other)
				{
					_hits += other._hits;
					_misses += other._misses;
					_hit_under_misses += other._hit_under_misses;
					_lse_stalls += other._lse_stalls;
					_mshr_stalls += other._mshr_stalls;
					_bank_conflicts += other._bank_conflicts;
				}

				void log_hit(bool under_miss = false) { _hits++; if(under_miss) _hit_under_misses++; } //TODO hit under miss logging
				void log_miss() { _misses++; }

				void log_bank_conflict() { _bank_conflicts++; } //TODO bank conflict logging
				void log_lse_stall() { _lse_stalls++; }
				void log_mshr_stall() { _mshr_stalls++; }


				void print_log(uint units = 1)
				{
					uint64_t total = _misses + _hits;
					float hit_rate = static_cast<float>(_hits) / total;
					float miss_rate = static_cast<float>(_misses) / total;

					printf("Total: %lld\n", total / units);
					printf("Hits: %lld\n", _hits / units);
					printf("Misses: %lld\n", _misses / units);
					printf("Hit Rate: %.2f%%\n", hit_rate * 100.0f);
					printf("Miss Rate: %.2f%%\n", miss_rate * 100.0f);
					printf("LSE Stalls: %lld\n", _lse_stalls / units);
					printf("MSHR Stalls: %lld\n", _mshr_stalls / units);
				}
			}log;

		private:
			struct _CacheLine
			{
				uint64_t tag     : 59;
				uint64_t lru     : 4;
				uint64_t valid   : 1;
			};

			struct MSHR
			{
				paddr_t line_paddr{0x0ull};
				bool valid{false};
				bool issued{false};
			};

			struct LSE
			{			
				uint mshr_index{~0u};
				uint request_index{~0u};
				bool valid{false};
				bool ready{false}; //returned from memory so we can start returning to mem lower
				MemoryRequestItem request;
			};

			struct _Bank
			{
				std::vector<MSHR> mshrs;
				std::vector<LSE>  lses;

				RoundRobinArbitrator request_arbitrator;
				//RoundRobinArbitrator return_arbitrator{NUM_LSE};

				MemoryRequestItem current_request; //current request
				uint cycles_remaining{0u};
				uint request_index{~0u};
				bool active{false};

				//only for blocking cache
				bool block_for_store{false};
				bool block_for_load{false};

				_Bank(uint num_ports, uint num_lse, uint num_mshr) : request_arbitrator(num_ports), lses(num_lse), mshrs(num_mshr) {}

				uint get_mshr(paddr_t line_paddr)
				{
					for(uint i = 0; i < mshrs.size(); ++i)
						if(mshrs[i].valid && mshrs[i].line_paddr == line_paddr) 
							return i;
					return ~0u;
				}

				uint get_free_mshr()
				{
					for(uint i = 0; i < mshrs.size(); ++i)
						if(!mshrs[i].valid)
							return i;
					return ~0u;
				}

				uint get_free_lse()
				{
					for(uint i = 0; i < lses.size(); ++i)
						if(!lses[i].valid)
							return i;
					return ~0u;
				}
			};

			Configuration _configuration; //nice for debugging

			bool _blocking;

			uint _set_index_offset, _tag_offset, _bank_index_offset;
			uint64_t _set_index_mask, _tag_mask, _bank_index_mask;

			uint _penalty, _associativity, _line_size;
			std::vector<_CacheLine> _cache_state;

			std::vector<_Bank> _banks;
			uint _num_incoming_connections;

			UnitMemoryBase* _mem_higher;
			uint _mem_higher_bank0_request_index;

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
			void _update_banks_fall_blocking();

			bool _get_cache_line(paddr_t paddr);
			void _insert_cache_line(paddr_t paddr);

			uint32_t _get_offset(paddr_t paddr) { return  static_cast<uint32_t>((paddr >> 0) & offset_mask); }
			uint32_t _get_set_index(paddr_t paddr) { return  static_cast<uint32_t>((paddr >> _set_index_offset) & _set_index_mask); }
			uint64_t _get_tag(paddr_t paddr) { return (paddr >> _tag_offset) & _tag_mask; }
			
			uint32_t _get_bank(paddr_t paddr) 
			{
				//todo better hash function
				return static_cast<uint32_t>((paddr >> _bank_index_offset) & _bank_index_mask); 
			}

			paddr_t _get_cache_line_start_paddr(paddr_t paddr) { return paddr & ~offset_mask; }
		};
	}
}
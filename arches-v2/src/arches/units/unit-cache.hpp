#pragma once 
#include "../../stdafx.hpp"

#include "../util/bit-manipulation.hpp"
#include "unit-base.hpp"
#include "unit-memory-base.hpp"

#define WCB_SIZE 16

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
				uint penalty{1};
				bool blocking{false};
				uint num_mshr{1};

				float dynamic_read_energy;
				float bank_leakge_power;
			};

			class Log
			{
			public:
				uint64_t _hits;
				uint64_t _half_miss;
				uint64_t _misses;

				uint64_t _mshr_stalls;

				uint64_t _bank_conflicts;

				Log() { reset(); }

				void reset()
				{
					_hits = 0;
					_misses = 0;
					_half_miss = 0;
					_mshr_stalls = 0;
					_bank_conflicts = 0;
				}

				void accumulate(const Log& other)
				{
					_hits += other._hits;
					_misses += other._misses;
					_half_miss += other._half_miss;;
					_mshr_stalls += other._mshr_stalls;
					_bank_conflicts += other._bank_conflicts;
				}

				void log_hit() { _hits++; } //TODO hit under miss logging
				void log_half_miss() { _half_miss++; }
				void log_miss() { _misses++; }

				void log_bank_conflict() { _bank_conflicts++; }
				void log_mshr_stall() { _mshr_stalls++; }

				uint64_t get_total() { return _misses + _hits; }

				void print_log(FILE* stream = stdout, uint units = 1)
				{
					uint64_t total = get_total();
					float hit_rate = static_cast<float>(_hits) / total;
					float miss_rate = static_cast<float>(_misses) / total;

					fprintf(stream, "Total: %lld\n", total / units);
					fprintf(stream, "Hits: %lld\n", _hits / units);
					fprintf(stream, "Misses: %lld\n", _misses / units);
					fprintf(stream, "Half Misses: %lld\n", _half_miss / units);
					fprintf(stream, "Hit Rate: %.2f%%\n", hit_rate * 100.0f);
					fprintf(stream, "Miss Rate: %.2f%%\n", miss_rate * 100.0f);
					fprintf(stream, "Bank Conflicts: %lld\n", _bank_conflicts / units);
					fprintf(stream, "MSHR Stalls: %lld\n", _mshr_stalls / units);
				}
			}log;

		private:
			struct _CacheLine
			{
				uint64_t tag     : 59;
				uint64_t lru     : 4;
				uint64_t valid   : 1;
			};

			struct WCB
			{
				paddr_t block_addr;
				uint64_t mask;
			};

			struct MSHR
			{
				paddr_t block_addr{0x0ull};
				uint64_t request_mask{0x0ull};

				enum
				{
					FREE,
					VALID,
					ISSUED,
				};

				uint8_t state{FREE};
			};

			struct _Bank
			{
				std::vector<MSHR> mshrs;
				WCB wcb;

				MemoryRequest request; //current request
				uint64_t request_mask{0x0ull};

				uint cycles_remaining{0u};
				bool busy{false};
				bool stalled_for_mshr{false};

				//only for blocking cache
				bool block_for_store{false};
				bool block_for_load{false};

				_Bank(uint num_mshr) { mshrs.resize(num_mshr); }

				uint get_mshr(paddr_t line_paddr, uint64_t req_mask = 0x0ull)
				{
					for(uint i = 0; i < mshrs.size(); ++i)
						if(mshrs[i].state != MSHR::FREE && mshrs[i].block_addr == line_paddr)
						{
							mshrs[i].request_mask |= req_mask;
							return i;
						}

					return ~0u;
				}

				uint allocate_mshr(paddr_t line_paddr, uint64_t req_mask)
				{
					for(uint i = 0; i < mshrs.size(); ++i)
						if(mshrs[i].state == MSHR::FREE)
						{
							mshrs[i].block_addr = line_paddr;
							mshrs[i].request_mask = req_mask;
							mshrs[i].state = MSHR::VALID;
							return i;
						}

					//failed to allocate mshrs are full
					return ~0u;
				}

				void free_mshr(uint index)
				{
					uint i;
					for(i = index; i < (mshrs.size() - 1); ++i)
						mshrs[i] = mshrs[i + 1];

					mshrs[i].state = MSHR::FREE;
				}
			};

			uint _bank_priority_index{0};
			uint _port_priority_index{0};

			Configuration _configuration; //nice for debugging

			bool _blocking;

			uint _set_index_offset, _tag_offset, _bank_index_offset;
			uint64_t _set_index_mask, _tag_mask, _bank_index_mask;

			uint _penalty, _associativity, _line_size;
			std::vector<_CacheLine> _cache_state;

			std::vector<_Bank> _banks;
			uint _num_incoming_connections;

			UnitMemoryBase* _mem_higher;
			uint _mem_higher_request_index;

		public:
			UnitCache(Configuration config, UnitMemoryBase* mem_higher, uint bus_index_start, Simulator* simulator);
			virtual ~UnitCache();
			void clock_rise() override;
			void clock_fall() override;

		private:
			void _proccess_load_return();
			void _proccess_requests();
			void _update_banks_fall();
			void _update_banks_fall_blocking();

			void _try_return(const MemoryRequest& rtrn, uint64_t& mask);
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

			paddr_t _get_block_addr(paddr_t paddr) { return paddr & ~offset_mask; }
		};
	}
}
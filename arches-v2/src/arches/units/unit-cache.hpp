#pragma once 
#include "../../stdafx.hpp"

#include "../util/bit-manipulation.hpp"
#include "unit-base.hpp"
#include "unit-memory-base.hpp"

#define WCB_SIZE 16

namespace Arches { namespace Units {

class UnitCache final : public UnitMemoryBase
{
public:
	struct Configuration
	{
		uint size{1024};
		uint block_size{CACHE_LINE_SIZE};
		uint associativity{1};

		uint penalty{1};

		uint num_ports{1};
		uint num_banks{1};
		uint num_mshr{1};

		MemoryUnitMap mem_map;

		float dynamic_read_energy;
		float bank_leakge_power;
	};

	UnitCache(Configuration config);
	virtual ~UnitCache();
	void clock_rise() override;
	void clock_fall() override;

private:
	struct _BlockMetaData
	{
		uint64_t tag     : 59;
		uint64_t lru     : 4;
		uint64_t valid   : 1;
	};

	struct alignas(CACHE_LINE_SIZE) _BlockData
	{
		uint8_t data[CACHE_LINE_SIZE];
	};

	struct _WCB
	{
		paddr_t block_addr;
		uint64_t mask{0x0ull};
		uint8_t data[CACHE_LINE_SIZE];

		bool get_next(uint8_t& offset, uint8_t& size)
		{
			//try to find largest contiguous alligned block to issue load
			size = CACHE_LINE_SIZE;
			while(size > 0)
			{
				uint64_t comb_mask = generate_nbit_mask(size);
				for(offset = 0; offset < CACHE_LINE_SIZE; offset += size)
				{
					if((comb_mask & mask) == comb_mask)
					{
						mask &= ~(comb_mask);
						return true;
					}

					comb_mask <<= size;
				}
				size /= 2;
			}

			return false;
		}
	};

	struct _MSHR
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
		std::vector<_MSHR> mshrs;
		_WCB wcb;

		MemoryRequest request; //current request
		uint64_t request_mask{0x0ull};

		uint cycles_remaining{0u};
		bool busy{false};
		bool stalled_for_mshr{false};

		_Bank(uint num_mshr) { mshrs.resize(num_mshr); }

		uint get_mshr(paddr_t line_paddr, uint64_t req_mask = 0x0ull);
		uint allocate_mshr(paddr_t line_paddr, uint64_t req_mask);
		void free_mshr(uint index);
	};

	Configuration _configuration; //nice for debugging

	uint _bank_priority_index{0};

	uint _set_index_offset, _tag_offset, _bank_index_offset;
	uint64_t _set_index_mask, _tag_mask, _bank_index_mask;

	uint _penalty, _associativity, _line_size;
	std::vector<_BlockMetaData> _tag_array;
	std::vector<_BlockData> _data_array;

	std::vector<_Bank> _banks;
	uint _num_ports;

	MemoryUnitMap _mem_map;

	void _proccess_returns();
	void _proccess_requests();

	void _try_issue_load_returns(uint bank_index);
	void _try_issue_stores(uint bank_index);
	void _try_issue_loads(uint bank_index);
	void _update_bank(uint bank_index);

	void _try_return_load(const MemoryRequest& rtrn, uint64_t& mask);
	void* _get_block(paddr_t paddr);
	void* _insert_block(paddr_t paddr, void* data);

	uint32_t _get_offset(paddr_t paddr) { return  static_cast<uint32_t>((paddr >> 0) & offset_mask); }
	uint32_t _get_set_index(paddr_t paddr) { return  static_cast<uint32_t>((paddr >> _set_index_offset) & _set_index_mask); }
	uint64_t _get_tag(paddr_t paddr) { return (paddr >> _tag_offset) & _tag_mask; }
	uint32_t _get_bank(paddr_t paddr) { return static_cast<uint32_t>((paddr >> _bank_index_offset) & _bank_index_mask); }
	paddr_t _get_block_addr(paddr_t paddr) { return paddr & ~offset_mask; }

public:
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
};

}}
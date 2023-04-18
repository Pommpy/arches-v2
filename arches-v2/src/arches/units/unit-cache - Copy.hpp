#pragma once 
#include "../../stdafx.hpp"

#include "../util/bit-manipulation.hpp"
#include "../util/round-robin-arbitrator.hpp"
#include "unit-base.hpp"
#include "unit-memory-base.hpp"


//maps input lines to output lines
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

protected:
	class Bank
	{
	private:
		struct WCB
		{
			uint64_t block_mask{0x0ull};
			paddr_t block_addr;
			uint8_t data[64];

			bool get_next(uint8_t& offset, uint8_t& size);
		};

		struct LSE
		{
			enum
			{
				FREE,
				VALID,
				ACTIVE,
				MISS,
				ISSUED,
				READY,
			};

			paddr_t block_addr{0};
			uint64_t req_mask{0};
			uint8_t offset{0};
			uint8_t size{0};
			uint8_t state{FREE};
		};

	public:
		uint incoming_request_port;
		uint incoming_return_port;

		paddr_t outgoing_request_address;
		uint outgoing_request_port;

		bool outgoing_return_pending;
		uint outgoing_request_port;

	private:
		uint _oldest_valid_lse{0};
		uint _oldest_lse{0};
		uint _next_lse{0};
		std::vector<LSE> lsq;
		
		WCB wcb;

		struct alignas(8)
		{
			uint8_t store_register[CACHE_LINE_SIZE];
		};

		bool busy;

	public:
		Bank(const UnitCache::Configuration& config) { lsq.resize(config.num_mshr); }

	private:
		bool _allocate_lse(const MemoryRequest& request, uint64_t request_mask);
	};

	struct BlockMetaData
	{
		uint64_t tag : 59;
		uint64_t lru : 4;
		uint64_t valid : 1;
	};

	struct alignas(CACHE_LINE_SIZE) BlockData
	{
		uint8_t data[CACHE_LINE_SIZE];
	};

	Configuration _configuration; //nice for debugging

	std::vector<BlockMetaData> _tag_array;
	std::vector<BlockData> _data_array;

	uint _penalty, _associativity, _line_size;
	uint _set_index_offset, _tag_offset, _bank_index_offset;
	uint64_t _set_index_mask, _tag_mask, _offset_mask, _bank_index_mask;

	void* _get_block(paddr_t paddr);
	void* _insert_block(paddr_t paddr, void* data);

	paddr_t _get_offset(paddr_t paddr) { return (paddr >> 0) & _offset_mask; }
	paddr_t _get_set_index(paddr_t paddr) { return  (paddr >> _set_index_offset) & _set_index_mask; }
	paddr_t _get_tag(paddr_t paddr) { return (paddr >> _tag_offset) & _tag_mask; }
	paddr_t _get_block_addr(paddr_t paddr) { return paddr & ~_offset_mask; }
	paddr_t _get_bank_index(paddr_t paddr) { return (paddr >> _bank_index_offset) & _bank_index_mask; }

	std::vector<Bank> _banks;
	MemoryUnitMap _mem_map;

	ArbitrationNetwork incoming_request_network;
	ArbitrationNetwork incoming_return_network;
	ArbitrationNetwork outgoing_return_network;
	ArbitrationNetwork outgoing_request_network;

	void _route_incoming_requests();
	void _route_incoming_returns();

	void _route_outgoing_requests();
	void _route_outgoing_returns();

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
#pragma once 
#include "../../stdafx.hpp"

#include "../util/bit-manipulation.hpp"
#include "../util/round-robin-arbitrator.hpp"
#include "unit-base.hpp"
#include "unit-memory-base.hpp"

namespace Arches { namespace Units {

class UnitCache final : public UnitMemoryBase
{
public:
	struct Configuration
	{
		uint size{1024};
		uint associativity{1};

		uint penalty{1};


		uint num_ports{1};
		uint port_size{CACHE_BLOCK_SIZE};
		uint num_banks{1};
		uint num_lfb{1};

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

	struct alignas(CACHE_BLOCK_SIZE) _BlockData
	{
		uint8_t bytes[CACHE_BLOCK_SIZE];
	};

	struct _LFB //Line Fill Buffer
	{
		enum class Type : uint8_t
		{
			READ,
			WRITE, //TODO: support write through. This will need to write the store to the buffer then load the background data and write it back to the tag array
			//we can reuse some of read logic to do this. It is basically a read that always needs to be commited at the end (hit or miss)
			WRITE_COMBINING,
		};

		enum class State : uint8_t
		{
			INVALID,
			EMPTY,
			MISSED,
			ISSUED,
			FILLED,
		};

		_BlockData block_data;
		addr_t block_addr{~0ull};

		union
		{
			uint64_t byte_mask;                                 //used by stores
			uint64_t pword_request_masks[CACHE_BLOCK_SIZE / 8]; //used by loads
		};

		uint8_t lru{0u};
		Type type{Type::READ};
		State state{State::INVALID};
		bool commited{false};
		bool returned{false};

		_LFB()
		{
			for(uint i = 0; i < CACHE_BLOCK_SIZE / 8; ++i)
				pword_request_masks[i] = 0;
		}

		bool operator==(const _LFB& other) const
		{
			return block_addr == other.block_addr && type == other.type;
		}

		uint64_t get_next(uint8_t& offset, uint8_t& size)
		{
			assert(type == _LFB::Type::WRITE_COMBINING);

			//try to find largest contiguous alligned block to issue store
			size = CACHE_BLOCK_SIZE;
			while(size > 0)
			{
				uint64_t comb_mask = generate_nbit_mask(size);
				for(offset = 0; offset < CACHE_BLOCK_SIZE; offset += size)
				{
					if((comb_mask & byte_mask) == comb_mask)
					{
						byte_mask &= ~(comb_mask);
						return 1;
					}

					comb_mask <<= size;
				}
				size /= 2;
			}

			return 0;
		}
	};

	struct _Bank
	{
		std::vector<_LFB> lfbs;

		uint cycles_remaining{0u};
		uint lfb_accessing_cache{~0u}; 

		uint outgoing_request_lfb{~0u};
		uint outgoing_request_byte_mask{0};

		uint outgoing_return_lfb{~0u};
		uint outgoing_return_pword_index{0u};

		uint outgoing_return_new_request_mask{0u};

		_Bank(uint num_lfb) : lfbs(num_lfb) {}
	};


	Configuration _configuration; //nice for debugging

	uint _bank_priority_index{0};

	uint _set_index_offset, _tag_offset, _bank_index_offset, _port_width, _pwords_per_block;
	uint64_t _set_index_mask, _tag_mask, _bank_index_mask, _block_offset_mask;

	uint _penalty, _associativity;
	std::vector<_BlockMetaData> _tag_array;
	std::vector<_BlockData> _data_array;

	std::vector<_Bank> _banks;
	MemoryUnitMap _mem_map;

	ArbitrationNetwork incoming_request_network;
	ArbitrationNetwork incoming_return_network;
	ArbitrationNetwork outgoing_return_network;
	ArbitrationNetwork outgoing_request_network;


	uint _fetch_lfb(uint bank_index, _LFB& lfb);
	uint _allocate_lfb(uint bank_index, _LFB& lfb);
	uint _fetch_or_allocate_lfb(uint bank_index, uint64_t block_addr, _LFB::Type type);

	void _update_interconection_network_rise();
	void _propagate_acknowledge_signals();
	void _update_interconection_network_fall();

	void _proccess_returns(uint bank_index);
	void _proccess_requests(uint bank_index);

	void _update_bank(uint bank_index);
	void _try_issue_lfb(uint bank_index);
	void _try_return_lfb(uint bank_index);

	_BlockData* _get_block(paddr_t paddr);
	_BlockData* _insert_block(paddr_t paddr, _BlockData& data);

	paddr_t _get_block_offset(paddr_t paddr) { return  (paddr >> 0) & _block_offset_mask; }
	paddr_t _get_block_addr(paddr_t paddr) { return paddr & ~_block_offset_mask; }
	paddr_t _get_bank_index(paddr_t paddr) { return (paddr >> _bank_index_offset) & _bank_index_mask; }
	paddr_t _get_set_index(paddr_t paddr) { return  (paddr >> _set_index_offset) & _set_index_mask; }
	paddr_t _get_tag(paddr_t paddr) { return (paddr >> _tag_offset) & _tag_mask; }

public:
	class Log
	{
	public:
		uint64_t _hits;
		uint64_t _half_miss;
		uint64_t _misses;
		uint64_t _lfb_stalls;
		uint64_t _bank_conflicts;

		Log() { reset(); }

		void reset()
		{
			_hits = 0;
			_misses = 0;
			_half_miss = 0;
			_lfb_stalls = 0;
			_bank_conflicts = 0;
		}

		void accumulate(const Log& other)
		{
			_hits += other._hits;
			_misses += other._misses;
			_half_miss += other._half_miss;;
			_lfb_stalls += other._lfb_stalls;
			_bank_conflicts += other._bank_conflicts;
		}

		void log_hit(uint n = 1) { _hits += n; } //TODO hit under miss logging
		void log_half_miss(uint n = 1) { _half_miss += n; }
		void log_miss(uint n = 1) { _misses += n; }

		void log_bank_conflict() { _bank_conflicts++; }
		void log_lfb_stall() { _lfb_stalls++; }

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
			fprintf(stream, "LFB Stalls: %lld\n", _lfb_stalls / units);
		}
	}log;
};

}}
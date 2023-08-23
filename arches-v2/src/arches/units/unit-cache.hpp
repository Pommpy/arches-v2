#pragma once 
#include "../../stdafx.hpp"

#include "../util/bit-manipulation.hpp"
#include "../util/arbitration.hpp"
#include "unit-base.hpp"
#include "unit-memory-base.hpp"

namespace Arches { namespace Units {


/*! \class UnitCache
	\brief Class defines cache memory configuration and its execution routines

*/
class UnitCache final : public UnitMemoryBase
{
public:

	/*! \struct Configuration
	*	\brief	struct describes cache configuration
	*
	*	TODO: long description
	*/
	struct Configuration
	{
		uint size{1024};
		uint associativity{1};

		uint tag_array_access_cycles{1};
		uint data_array_access_cycles{1};

		uint num_ports{1};
		uint port_size{CACHE_BLOCK_SIZE};
		uint num_banks{1};
		uint num_lfb{1};

		MemoryMap mem_map;

		float dynamic_read_energy;
		float bank_leakge_power;
	};

	UnitCache(Configuration config);
	virtual ~UnitCache();
	void clock_rise() override;
	void clock_fall() override;

private:
	/*! \struct _BlockMetaData
	*	\brief	struct describes tag, lru and valid bit sizes
	*
	*	TODO: long description
	*/
	struct _BlockMetaData
	{
		uint64_t tag     : 59;
		uint64_t lru     : 4;
		uint64_t valid   : 1;
	};

	/*! \struct _BlockData
	*	\brief	struct describes total byte size of cache
	*/
	struct alignas(CACHE_BLOCK_SIZE) _BlockData
	{
		uint8_t bytes[CACHE_BLOCK_SIZE];
	};


	/*! \struct _LFB
	*	\brief	Line Fill Buffer: TODO
	*/
	struct _LFB 
	{
		enum class Type : uint8_t
		{
			READ,
			WRITE, //TODO: support write through. This will need to write the store to the buffer then load the background data and write it back to the tag array
			//we can reuse some of read logic to do this. It is basically a read that always needs to be commited at the end (hit or miss).
			//in the furture we might also want to support cache coherency
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
			uint64_t chunk_request_masks[CACHE_BLOCK_SIZE / 8]; //used by loads
		};

		uint8_t lru{0u};
		Type type{Type::READ};
		State state{State::INVALID};
		bool commited{false};
		bool returned{false};

		_LFB()
		{
			for(uint i = 0; i < CACHE_BLOCK_SIZE / 8; ++i)
				chunk_request_masks[i] = 0;
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

	/*! \struct _Bank
	*	\brief	Describes behavior of banks within the cache
	*
	*	TODO: long description
	*/
	struct _Bank
	{
		std::vector<_LFB> lfbs;

		uint cycles_remaining{0u};
		uint lfb_accessing_cache{~0u}; 

		uint outgoing_request_lfb{~0u};
		uint64_t outgoing_request_byte_mask{0x0ull};

		uint outgoing_return_lfb{~0u};
		uint outgoing_return_chunk_index{0u};

		_Bank(uint num_lfb) : lfbs(num_lfb) {}
	};


	Configuration _configuration; //nice for debugging

	uint _bank_priority_index{0};

	uint _set_index_offset, _tag_offset, _bank_index_offset, _chunk_width, _chunks_per_block;
	uint64_t _set_index_mask, _tag_mask, _bank_index_mask, _block_offset_mask;

	uint _data_array_access_cycles, _tag_array_access_cycles, _associativity;
	std::vector<_BlockMetaData> _tag_array;
	std::vector<_BlockData> _data_array;

	std::vector<_Bank> _banks;
	MemoryMap _mem_map;

	ArbitrationNetwork64 incoming_return_network;
	ArbitrationNetwork64 outgoing_request_network;

	uint _fetch_lfb(uint bank_index, _LFB& lfb);
	uint _allocate_lfb(uint bank_index, _LFB& lfb);
	uint _fetch_or_allocate_lfb(uint bank_index, uint64_t block_addr, _LFB::Type type);

	void _interconnects_rise();
	void _propagate_ack();
	void _interconnects_fall();

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

	/*! \struct Log
	*	\brief	Logger class that logs debug info to track execution
	*/
	class Log
	{
	public:
		uint64_t _total;
		uint64_t _hits;
		uint64_t _misses;
		uint64_t _lfb_hits;
		uint64_t _half_misses;
		uint64_t _bank_conflicts;
		uint64_t _lfb_stalls;
		uint64_t _data_array_reads;
		uint64_t _data_array_writes;

		Log() { reset(); }

		void reset()
		{
			_total = 0;
			_lfb_hits = 0;
			_hits = 0;
			_misses = 0;
			_half_misses = 0;
			_bank_conflicts = 0;
			_lfb_stalls = 0;
			_data_array_reads = 0;
			_data_array_writes = 0;
		}

		void accumulate(const Log& other)
		{
			_total += other._total;
			_lfb_hits += other._lfb_hits;
			_hits += other._hits;
			_misses += other._misses;
			_half_misses += other._half_misses;;
			_bank_conflicts += other._bank_conflicts;
			_lfb_stalls += other._lfb_stalls;
			_data_array_reads += other._data_array_reads;
			_data_array_writes += other._data_array_writes;
		}

		void log_requests(uint n = 1) { _total += n; } //TODO hit under miss logging

		void log_hit(uint n = 1) { _hits += n; } //TODO hit under miss logging
		void log_miss(uint n = 1) { _misses += n; }

		void log_lfb_hit(uint n = 1) { _lfb_hits += n; }
		void log_half_miss(uint n = 1) { _half_misses += n; }

		void log_bank_conflict() { _bank_conflicts++; }
		void log_lfb_stall() { _lfb_stalls++; }

		void log_data_array_read() { _data_array_reads++; }
		void log_data_array_write() { _data_array_writes++; }

		uint64_t get_total() { return _hits + _misses + _lfb_hits + _half_misses; }
		uint64_t get_total_data_array_accesses() { return _data_array_reads + _data_array_writes; }

		void print_log(FILE* stream = stdout, uint units = 1)
		{
			uint64_t total = get_total();
			float ft = total / 100.0f;

			uint64_t da_total = get_total_data_array_accesses();

			fprintf(stream, "Total: %lld\n", total / units);
			fprintf(stream, "Total2: %lld\n", _total / units);
			fprintf(stream, "Hits: %lld(%.2f%%)\n", _hits / units, _hits / ft);
			fprintf(stream, "Misses: %lld(%.2f%%)\n", _misses / units, _misses / ft);
			fprintf(stream, "LFB Hits: %lld(%.2f%%)\n", _lfb_hits / units, _lfb_hits / ft);
			fprintf(stream, "Half Misses: %lld(%.2f%%)\n", _half_misses / units, _half_misses / ft);
			fprintf(stream, "Bank Conflicts: %lld\n", _bank_conflicts / units);
			fprintf(stream, "LFB Stalls: %lld\n", _lfb_stalls / units);
			fprintf(stream, "Data Array Total: %lld\n", da_total);
			fprintf(stream, "Data Array Reads: %lld\n", _data_array_reads);
			fprintf(stream, "Data Array Writes: %lld\n", _data_array_writes);
		}
	}log;
};

}}
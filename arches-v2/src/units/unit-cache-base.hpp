#pragma once 
#include "../stdafx.hpp"

#include "unit-memory-base.hpp"
#include "../util/bit-manipulation.hpp"

namespace Arches { namespace Units {

class UnitCacheBase : public UnitMemoryBase
{
public:
	UnitCacheBase(size_t size, uint associativity);
	virtual ~UnitCacheBase();

protected:
	class CacheRequestCrossBar : public CasscadedCrossBar<MemoryRequest>
	{
	private:
		uint64_t mask;

	public:
		CacheRequestCrossBar(uint ports, uint banks, uint64_t bank_select_mask) : mask(bank_select_mask), CasscadedCrossBar<MemoryRequest>(ports, banks, banks) {}

		uint get_sink(const MemoryRequest& request) override
		{
			uint bank = pext(request.paddr, mask);
			assert(bank < num_sinks());
			return bank;
		}
	};

	class CacheReturnCrossBar : public CasscadedCrossBar<MemoryReturn>
	{
	public:
		CacheReturnCrossBar(uint ports, uint banks) : CasscadedCrossBar<MemoryReturn>(banks, ports, banks) {}

		uint get_sink(const MemoryReturn& ret) override
		{
			return ret.port;
		}
	};

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

	uint64_t _set_index_mask, _tag_mask, _block_offset_mask;
	uint _set_index_offset, _tag_offset;

	uint _associativity;
	std::vector<_BlockMetaData> _tag_array;
	std::vector<_BlockData> _data_array;

	_BlockData* _get_block(paddr_t paddr);
	_BlockData* _insert_block(paddr_t paddr, const uint8_t* data);

	paddr_t _get_block_offset(paddr_t paddr) { return  (paddr >> 0) & _block_offset_mask; }
	paddr_t _get_block_addr(paddr_t paddr) { return paddr & ~_block_offset_mask; }
	paddr_t _get_set_index(paddr_t paddr) { return  (paddr >> _set_index_offset) & _set_index_mask; }
	paddr_t _get_tag(paddr_t paddr) { return (paddr >> _tag_offset) & _tag_mask; }
};

}}
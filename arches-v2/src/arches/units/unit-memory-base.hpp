#pragma once

#include "../../stdafx.hpp"

#include "unit-base.hpp"

namespace Arches { namespace Units {

#define CACHE_LINE_SIZE 32

struct MemoryRequest
{
	enum class Type : uint8_t
	{
		STORE,
		LOAD,
		LOAD_RETURN,

		AMO_LOAD,
		AMO_ADD,
		AMO_RETURN,
	};

	//meta data 
	Type    type;
	uint8_t size;

	union
	{
		paddr_t paddr;
		vaddr_t vaddr;
	};

	union
	{
		uint8_t _data[8];

		uint8_t  data_u8;
		uint16_t data_u16;
		uint32_t data_u32;
		uint64_t data_u64;

		float    data_f32;
		double   data_f64;
	};
};

class UnitMemoryBase : public UnitBase
{
public:
	ConnectionGroup<MemoryRequest> request_bus;
	ConnectionGroup<MemoryRequest> return_bus;
	uint                        offset_bits{ 0 }; //how many bits are used for the offset. Needed by the core to align loads to line boundries properly
	uint64_t                    offset_mask{ 0 };

	UnitMemoryBase(uint num_clients, Simulator* simulator) : request_bus(num_clients), return_bus(num_clients), UnitBase(simulator)
 	{
		
	}
};


class MemoryUnitMap
{
public:

	struct MemoryUnitMapping
	{
		UnitMemoryBase* unit;
		uint offset;
	};

	std::vector<paddr_t> ranges;
	std::vector<MemoryUnitMapping> units;

	void add_unit(paddr_t paddr, UnitMemoryBase* unit, uint offset)
	{
		uint i = 0;
		for(; i < ranges.size(); ++i)
			if(paddr < ranges[i]) break;

		ranges.insert(ranges.begin() + i, paddr);
		units.insert(units.begin() + i, {unit, offset});
	}

	uint get_index(paddr_t paddr)
	{
		uint start = 0;
		uint end = ranges.size();
		while((start + 1) != end)
		{
			uint middle = (start + end) / 2;
			if(paddr >= ranges[middle]) start = middle;
			else                        end = middle;
		}

		return start;
	}

	UnitMemoryBase* get_unit(uint index)
	{
		return units[index].unit;
	}

	uint get_offset(uint index)
	{
		return units[index].offset;
	}
};

}}



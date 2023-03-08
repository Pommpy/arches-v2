#pragma once

#include "../../stdafx.hpp"

#include "unit-base.hpp"

namespace Arches { namespace Units {

#define CACHE_LINE_SIZE 32

struct MemoryRequest
{
	enum class Type : uint8_t
	{
		//must match the first part of ISA::RISCV::Type f
		//TODO: We should figure a way to make this and the isa RISCV enum the same structure. Right now this isn't maintainable. Also it should be impmnetation dependent somehow so that we can have diffrent isntruction based on impl.

		NA,

		LOAD,
		STORE,

		AMO_ADD,
		AMO_XOR,
		AMO_OR,
		AMO_AND,
		AMO_MIN,
		AMO_MAX,
		AMO_MINU,
		AMO_MAXU,

		TRAXAMOIN,
		LBRAY,
		SBRAY,
		CSHIT,

		//other non instruction mem ops
		LOAD_RETURN,
	};

	//meta data 
	Type    type;
	uint8_t size;

	union
	{
		paddr_t paddr;
		vaddr_t vaddr;
	};

	//Only atomic instructions actually pass or recive data.
	//The data pointed to by this must precist until the pending line in cleared.
	//Coppying data into the request would be very inefficent (especially when passing full blocks of data).
	//Hence why we handle it by passing a pointer the data can then be coppied directly by the reciver
	void* data{nullptr};
};

class UnitMemoryBase : public UnitBase
{
public:
	ConnectionGroup<MemoryRequest> request_bus;
	ConnectionGroup<MemoryRequest> return_bus;
	uint                        offset_bits{ 0 }; //how many bits are used for the offset. Needed by the core to align loads to line boundries properly
	uint64_t                    offset_mask{ 0 };

	UnitMemoryBase(uint num_clients) : request_bus(num_clients), return_bus(num_clients), UnitBase()
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
			else                        end   = middle;
		}

		return start;
	}

	UnitMemoryBase* get_unit(uint index)
	{
		return units[index].unit;
	}

	uint get_port(uint index)
	{
		return units[index].offset;
	}
};

}}



#pragma once

#include "../../stdafx.hpp"

#include "unit-base.hpp"

namespace Arches { namespace Units {

#define CACHE_LINE_SIZE 64

struct MemoryRequestItem
{
	enum class Type : uint8_t
	{
		STORE,
		LOAD,
		LOAD_RETURN,
		AMOADD,
	};

	//meta data 
	Type    type{Type::STORE};
	uint8_t size{0};
	uint8_t offset{0};

	bool    sign_extend{false};
	uint8_t dst_reg{0};
	uint8_t dst_reg_file{0};

	bool contains_data{false};

	paddr_t line_paddr{0ull};

	union
	{
		uint8_t  data_u8;
		uint16_t data_u16;
		uint32_t data_u32;
		uint64_t data_u64;
		float    data_f32;
		double   data_f64;
		void*    data_;
	};
};

class UnitMemoryBase : public UnitBase
{
public:
	ConnectionGroup<MemoryRequestItem> request_bus;
	ConnectionGroup<MemoryRequestItem> return_bus;
	uint                        offset_bits{ 0 }; //how many bits are used for the offset. Needed by the core to align loads to line boundries properly
	uint64_t                    offset_mask{ 0 };

	UnitMemoryBase(uint num_clients, Simulator* simulator) : request_bus(num_clients), return_bus(num_clients), UnitBase(simulator)
 	{
		
	}
};


class MemoryUnitMap
{
	std::vector<paddr_t>         ranges;
	std::vector<UnitMemoryBase*> units;

public:
	void add_unit(paddr_t paddr, UnitMemoryBase* unit)
	{
		uint i = 0;
		for(; i < ranges.size(); ++i)
			if(paddr < ranges[i]) break;

		ranges.insert(ranges.begin() + i, paddr);
		units.insert(units.begin() + i, unit);
	}

	UnitMemoryBase* get_unit(paddr_t paddr)
	{
		uint start = 0;
		uint end = ranges.size();
		while((start + 1) != end)
		{
			uint middle = (start + end) / 2;
			if(paddr >= ranges[middle]) start = middle;
			else                        end = middle;
		}

		return units[start];
	}
};

}}



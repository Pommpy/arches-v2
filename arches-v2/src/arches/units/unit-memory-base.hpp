#pragma once

#include "../../stdafx.hpp"

#include "unit-base.hpp"

namespace Arches { namespace Units {

#define CACHE_BLOCK_SIZE 32

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

		FCHTHRD,
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

	bool operator==(const MemoryRequest& other) const
	{
		return type == other.type && size == other.size && paddr == other.paddr;
	}

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

	UnitMemoryBase(uint num_clients) : request_bus(num_clients), return_bus(num_clients), UnitBase()
 	{
		
	}
};


class MemoryUnitMap
{
private:
	std::vector<std::pair<paddr_t, uint>> ranges;

public:
	struct MemoryUnitMapping
	{
		UnitMemoryBase* unit;
		uint16_t        port_index;
		uint16_t        num_ports;
		uint16_t        port_id; //provides unqie ids for all ports

		bool operator==(const MemoryUnitMapping& other) const
		{
			return unit == other.unit && port_index == other.port_index && num_ports == other.num_ports;
		}
	};

	std::vector<MemoryUnitMapping> mappings;
	uint total_ports{0};

	void add_unit(paddr_t paddr, UnitMemoryBase* unit, uint port_id, uint num_ports)
	{
		if(unit == nullptr)
		{
			num_ports = 0;
			port_id = 0;
		}

		uint i = 0;
		for(; i < ranges.size(); ++i)
			if(paddr < ranges[i].first) break;

		ranges.insert(ranges.begin() + i, {paddr, ~0u});

		if(unit == nullptr) return;

		MemoryUnitMapping mapping = { unit, port_id, num_ports, 0};

		uint j;
		for(j = 0; j < mappings.size(); ++j)
			if(mapping == mappings[j]) break;

		if(j == mappings.size()) mappings.push_back(mapping);

		ranges[i].second = j;

		total_ports = 0;
		for(j = 0; j < mappings.size(); ++j)
		{
			mappings[j].port_id = total_ports;
			total_ports += mappings[j].num_ports;
		}
	}

	uint get_mapping_index(paddr_t paddr)
	{
		uint start = 0;
		uint end = ranges.size();
		while((start + 1) != end)
		{
			uint middle = (start + end) / 2;
			if(paddr >= ranges[middle].first) start = middle;
			else                              end   = middle;
		}

		return ranges[start].second;
	}

	uint get_mapping_index_for_unique_port_index(uint unique_port_index)
	{
		uint start = 0;
		uint end = mappings.size();
		while((start + 1) != end)
		{
			uint middle = (start + end) / 2;
			if(unique_port_index >= mappings[middle].port_id) start = middle;
			else                                                     end = middle;
		}

		return start;
	}
};

}}



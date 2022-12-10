#pragma once 
#include "../../stdafx.hpp"

#include "unit-base.hpp"
#include "unit-memory-base.hpp"

namespace Arches { namespace Units {

class UnitAtomicRegfile : public UnitMemoryBase
{
public:
	uint32_t iregs[32];

	UnitAtomicRegfile(uint num_clients, Simulator* simulator) : arbitrator(num_clients), UnitMemoryBase(num_clients, simulator)
	{
		executing = false;
		for(uint i = 0; i < 32; ++i)
			iregs[i] = 0;
	}

	RoundRobinArbitrator arbitrator;

	uint request_index{~0u};
	MemoryRequestItem request_item;

	void clock_rise() override
	{
		for(uint i = 0; i < request_bus.size(); ++i)
			if(request_bus.get_pending(i)) arbitrator.push_request(i);

		if((request_index = arbitrator.pop_request()) != ~0)
		{
			request_item = request_bus.get_data(request_index);
			request_bus.clear_pending(request_index);
		}
	}

	void clock_fall() override
	{
		if(request_index != ~0)
		{
			uint32_t reg_index = request_item.line_paddr >> 2 & 0b1'1111;

			switch(request_item.type)
			{
			case MemoryRequestItem::Type::STORE:
				iregs[reg_index] = request_item.data_u32;
				break;

			case MemoryRequestItem::Type::LOAD:
				request_item.data_u32 = iregs[reg_index];
				break;

			case MemoryRequestItem::Type::AMOADD:
				printf(" amoadd: %d\r", iregs[reg_index]);
				uint tmp = request_item.data_u32;
				request_item.data_u32 = iregs[reg_index];
				iregs[reg_index] += tmp;
				break;
			}

			if(request_item.type != MemoryRequestItem::Type::STORE)
			{
				request_item.type = MemoryRequestItem::Type::LOAD_RETURN;
				return_bus.set_data(request_item, request_index);
				return_bus.set_pending(request_index);
			}
		}
	}
};

}}
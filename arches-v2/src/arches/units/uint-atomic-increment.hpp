#pragma once 
#include "../../stdafx.hpp"

#include "unit-base.hpp"
#include "unit-memory-base.hpp"

namespace Arches { namespace Units {

class UnitAtomicIncrement : public UnitMemoryBase
{
public:
	UnitAtomicIncrement(uint num_clients, Simulator* simulator) : arbitrator(num_clients), UnitMemoryBase(num_clients, simulator)
	{
		executing = false;
	}

	uint32_t counter{0};
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
			assert(request_item.type == MemoryRequestItem::Type::LOAD);
			request_bus.clear_pending(request_index);
		}
	}

	void clock_fall() override
	{
		if(request_index != ~0)
		{
			if((counter & 0x00) == 0x0) printf(" Amoin: %d\r", counter);
			request_item.line_paddr = counter++;
			request_item.type = MemoryRequestItem::Type::LOAD_RETURN;

			return_bus.set_data(request_item, request_index);
			return_bus.set_pending(request_index);
		}
	}
};

}}
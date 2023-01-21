#pragma once 
#include "../../stdafx.hpp"

#include "unit-base.hpp"
#include "unit-memory-base.hpp"

namespace Arches { namespace Units {

class UnitBuffer : public UnitMemoryBase
{
public:
	UnitBuffer(uint num_clients, uint latencty, Simulator* simulator) : arbitrator(num_clients), UnitMemoryBase(num_clients, simulator)
	{
		executing = false;
	}

	RoundRobinArbitrator arbitrator;

	uint request_index{~0u};
	MemoryRequest request_item;

	void clock_rise() override
	{
		for(uint i = 0; i < request_bus.size(); ++i)
			if(request_bus.get_pending(i)) arbitrator.push_request(i);

		if((request_index = arbitrator.pop_request()) != ~0)
		{
			request_item = request_bus.get_data(request_index);
			assert(request_item.type == MemoryRequest::Type::LOAD);
			request_bus.clear_pending(request_index);
		}
	}

	void clock_fall() override
	{
		if(request_index != ~0)
		{
			request_item.type = MemoryRequest::Type::LOAD_RETURN;
			return_bus.set_data(request_item, request_index);
			return_bus.set_pending(request_index);
		}
	}
};

}}
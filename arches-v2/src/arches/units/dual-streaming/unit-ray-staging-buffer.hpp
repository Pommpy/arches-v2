#pragma once 
#include "../../../stdafx.hpp"

#include "../unit-base.hpp"
#include "../unit-memory-base.hpp"

#include "../../../../benchmarks/dual-streaming/src/ray.hpp"

namespace Arches { namespace Units {

class UnitRayStagingBuffer : public UnitMemoryBase
{
public:
	UnitRayStagingBuffer(uint num_clients, uint latencty) : UnitMemoryBase(num_clients), arbitrator(request_bus.pending, num_clients)
	{
		executing = false;
	}

	uint request_index{~0u};
	MemoryRequest request_item;
	RoundRobinArbitrator arbitrator;

	void clock_rise() override
	{
		request_index = arbitrator.next();
		if(request_index != ~0)
		{
			request_item = request_bus.transfer(request_index);
			request_bus.acknowlege(request_index);
		}
	}

	void clock_fall() override
	{
		if(request_index != ~0)
		{
			if(request_item.type == MemoryRequest::Type::LBRAY)
			{
				__debugbreak();
			}
			else if(request_item.type == MemoryRequest::Type::SBRAY) //ray write
			{
				__debugbreak();
			}
		}
	}
};

}}
#pragma once 
#include "../../../stdafx.hpp"

#include "../unit-base.hpp"
#include "../unit-memory-base.hpp"

#include "../../../../benchmarks/dual-streaming/src/ray.hpp"

namespace Arches { namespace Units {

class UnitRayStagingBuffer : public UnitMemoryBase
{
public:
	UnitRayStagingBuffer(uint num_clients, uint latencty, Simulator* simulator) : arbitrator(num_clients), UnitMemoryBase(num_clients, simulator)
	{
		executing = false;
	}

	RoundRobinArbitrator arbitrator;

	uint request_index{~0u};
	MemoryRequest request_item;

	uint store_index{~0u};
	uint stores{0};

	uint ray_buffer[9];

	void clock_rise() override
	{
		//we are midway through ray write stream
		if(store_index != ~0u)
		{
			if(request_bus.get_pending(store_index))
				request_index = store_index;
		}
		else
		{
			for(uint i = 0; i < request_bus.size(); ++i)
				if(request_bus.get_pending(i)) arbitrator.push_request(i);

			request_index = arbitrator.pop_request();
		}

		if(request_index != ~0)
		{
			request_item = request_bus.get_data(request_index);
			request_bus.clear_pending(request_index);

			if(request_item.type == MemoryRequest::Type::STORE)
			{
				stores++;
				if(stores == 1) store_index = request_index;
				if(stores == 9) store_index = ~0u;
			}
		}
	}

	void clock_fall() override
	{
		if(request_index != ~0)
		{
			if(request_item.type == MemoryRequest::Type::LOAD)
			{
				request_item.type = MemoryRequest::Type::LOAD_RETURN;
				return_bus.set_data(request_item, request_index);
				return_bus.set_pending(request_index);
			}
			else if(request_item.type == MemoryRequest::Type::STORE) //ray write
			{
				uint offset = request_item.line_paddr & 0xfff / 4;
				assert(offset < 9);
				ray_buffer[offset] = request_item.data_u32;

				if(stores == 9)
				{
					//TODO issue ray store to stream scheduler
					stores = 0;
					request_item.size = 9 * 4;
					request_item.data_ = ray_buffer;
				}
			}
		}
	}
};

}}
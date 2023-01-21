#pragma once 
#include "../../../stdafx.hpp"

#include "../unit-base.hpp"
#include "../unit-memory-base.hpp"

#include "unit-stream-scheduler.hpp"

namespace Arches { namespace Units { namespace DualStreaming {

class UnitSceneBuffer : public UnitMemoryBase
{
public:
	UnitSceneBuffer(uint num_clients, UnitStreamScheduler* stream_scheduler, Simulator* simulator) : 
		stream_scheduler(stream_scheduler), arbitrator(num_clients), UnitMemoryBase(num_clients, simulator)
	{
		executing = false;
		segment_map.resize(64, ~0ull);
	}

	RoundRobinArbitrator arbitrator;

	uint request_index{~0u};
	MemoryRequest request_item;

	std::set<paddr_t> segments;
	std::vector<paddr_t> segment_map;

	UnitStreamScheduler* stream_scheduler;
	uint stream_scheduler_index;

	void clock_rise() override
	{
		for(uint i = 0; i < request_bus.size(); ++i)
			if(request_bus.get_pending(i)) arbitrator.push_request(i);

		if((request_index = arbitrator.pop_request()) != ~0)
		{
			request_item = request_bus.get_data(request_index);
			request_bus.clear_pending(request_index);
		}

		if(stream_scheduler->return_bus.get_pending(stream_scheduler_index))
		{
			stream_scheduler->return_bus.clear_pending(stream_scheduler_index);
			paddr_t line_addr = stream_scheduler->return_bus.get_data(stream_scheduler_index).line_paddr;
			uint index = stream_scheduler->return_bus.get_data(stream_scheduler_index).data_u32;

			//todo remove the segment its replacing
			if((line_addr & 0xffff) == 0ull)
			{
				segments.erase(segment_map[index]);
				segment_map[index] = line_addr;
				segments.insert(line_addr);
			}
		}
	}

	void clock_fall() override
	{
		if(request_index != ~0)
		{
			assert(request_item.type == MemoryRequest::Type::LOAD);
			assert(segments.contains(request_item.line_paddr & 0xffff));

			request_item.type = MemoryRequest::Type::LOAD_RETURN;
			return_bus.set_data(request_item, request_index);
			return_bus.set_pending(request_index);
		}
	}
};

}}}
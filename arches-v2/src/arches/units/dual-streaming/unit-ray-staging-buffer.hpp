#pragma once 
#include "../../../stdafx.hpp"

#include "../unit-base.hpp"
#include "../unit-memory-base.hpp"
#include "unit-stream-scheduler.hpp"

#include "../../../../benchmarks/dual-streaming/src/ray.hpp"


namespace Arches { namespace Units { namespace DualStreaming {


struct RayBucketBuffer
{
	union
	{
		RayBucket ray_bucket;
		uint8_t data_u8[RAY_BUCKET_SIZE];
	};

	bool requested{false};
	uint next_ray{0};
	uint bytes_returned{0};

	RayBucketBuffer() 
	{
		ray_bucket.num_rays = 0;
	}
};

class UnitRayStagingBuffer : public UnitMemoryBase
{
public:
	UnitRayStagingBuffer(uint num_tp, uint tm_index, UnitStreamScheduler* stream_scheduler) : UnitMemoryBase(num_tp, 1), tm_index(tm_index)
	{
		front_buffer = &ray_buffer[0];
		back_buffer = &ray_buffer[1];
	}

	UnitStreamScheduler* stream_scheduler;

	uint tm_index;

	RayBucketBuffer* front_buffer;
	RayBucketBuffer* back_buffer;
	RayBucketBuffer ray_buffer[2];

	uint request_index{~0u};
	MemoryRequest request;

	void clock_rise() override
	{
		//requests
		interconnect.propagate_requests([](const MemoryRequest& req, uint client, uint num_clients, uint num_servers)->uint
		{
			return 0;
		});

		if(request_index == ~0u) return;

		uint port_index;
		const MemoryRequest* req = interconnect.get_request(0, port_index);
		if(req)
		{
			request = *req;
			request_index = port_index;
			interconnect.acknowlege_request(0, port_index);
		}

		//returns
		const MemoryReturn* ret = stream_scheduler->interconnect.get_return(tm_index);
		if(ret)
		{
			uint buffer_address = ret->paddr % RAY_BUCKET_SIZE;
			std::memcpy(&back_buffer->data_u8[buffer_address], ret->data, ret->size);
			stream_scheduler->interconnect.acknowlege_return(tm_index);

			back_buffer->bytes_returned += ret->size;
		}
	}

	void clock_fall() override
	{
		interconnect.propagate_ack();

		//requests
		if(front_buffer->next_ray == front_buffer->ray_bucket.num_rays &&
		   back_buffer->bytes_returned == RAY_BUCKET_SIZE)
		{
			//swap buffers
			std::swap(front_buffer, back_buffer);
		}

		if(!back_buffer->requested)
		{
			MemoryRequest req;
			req.type = MemoryRequest::Type::LOAD_RAY_BUCKET;
			req.size = RAY_BUCKET_SIZE;
			req.paddr = 0x0;
			stream_scheduler->interconnect.add_request(req, tm_index);
			back_buffer->requested = true;
		}

		//returns
		if(request_index == ~0u) return;

		if(request.type == MemoryRequest::Type::LBRAY)
		{
			//if we have a filled bucket pop a ray from it otherwise stall
			if(front_buffer->next_ray < front_buffer->ray_bucket.num_rays)
			{
				MemoryReturn ret;
				ret.type == MemoryReturn::Type::LOAD_RETURN;
				ret.size = sizeof(BucketRay);
				ret.paddr = 0x0ull;
				ret.data = &front_buffer->ray_bucket.bucket_rays[front_buffer->next_ray];
				interconnect.add_return(ret, 0, request_index);

				front_buffer->next_ray++;
			}
		}
		else if(request.type == MemoryRequest::Type::SBRAY || request.type == MemoryRequest::Type::CSHIT) //ray write or conditionally store hit
		{
			//forward to stream scheduler
			if(!stream_scheduler->interconnect.request_pending(tm_index))
			{
				stream_scheduler->interconnect.add_request(request, tm_index);
			}
		}

		interconnect.propagate_returns();
	}
};

}}}
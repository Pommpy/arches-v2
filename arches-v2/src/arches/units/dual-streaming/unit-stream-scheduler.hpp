#pragma once 
#include "../../../stdafx.hpp"

#include "../unit-base.hpp"
#include "../unit-memory-base.hpp"
#include "../unit-main-memory-base.hpp"
#include "../unit-buffer.hpp"

#include "../../../../benchmarks/dual-streaming/src/ray.hpp"

namespace Arches { namespace Units { namespace DualStreaming {

class UnitStreamScheduler : public UnitMemoryBase
{
public:
	struct Config
	{
		paddr_t scene_start;
		paddr_t bucket_start;
		uint bucket_size;
		uint segment_size;
		uint ray_size;
	};

	struct RayWriteQueueEntry
	{
		uint segment;
		BucketRay bucket_ray;
	};

	struct RayReadQueueEntry
	{
		uint tm;
		uint last_segment;
	};

	struct SceneSegmentState
	{
		bool in_buffer;
		bool drained;

		uint buckets_in_flight;

		uint child_index;
		uint num_children;
	};

	struct BucketState
	{
		uint fill;
		paddr_t address;
	};

	std::queue<RayWriteQueueEntry> ray_write_queue;
	std::queue<RayReadQueueEntry>  ray_read_queue;

	std::stack<paddr_t>         free_buckets; //todo this should be a linked list in memory
	std::map<uint, BucketState> bucket_states;
	std::map<uint, paddr_t>     first_bucket;

	std::queue<uint>                  eligable_segments;
	std::vector<uint>                 active_segments;
	std::map<uint, SceneSegmentState> segment_states;

	struct
	{
		paddr_t base_address;
		uint offset;

		uint scene_segment;
	}scene_stream;

	struct
	{
		paddr_t base_address;
		uint offset;

		uint scene_segment;
		uint tm;
	} ray_bucket_stream;

	struct
	{
		paddr_t base_address;
		uint    offset;
	} ray_write_stream;

	RoundRobinArbitrator request_arbitrator;

	UnitMainMemoryBase* main_mem;

	paddr_t scene_segments;
	paddr_t next_bucket;

	bool has_segment_return;
	bool has_bucket_return;

	MemoryRequestItem segment_return;
	MemoryRequestItem bucket_return;

	void process_load_returns()
	{
		//accept load returns
		if(main_mem->return_bus.get_pending(0) && !has_segment_return)
		{
			main_mem->return_bus.clear_pending(0);

			has_segment_return = true;
			segment_return = main_mem->request_bus.get_data(0);
		}

		if(main_mem->return_bus.get_pending(1) && !has_bucket_return)
		{
			main_mem->return_bus.clear_pending(1);

			has_bucket_return = true;
			bucket_return = main_mem->return_bus.get_data(1);
		}
	}

	void process_requests()
	{
		//check for requests to fill an empty staging buffer or write a ray
		for(uint i = 0; i < request_bus.size(); ++i)
			if(request_bus.get_pending(i)) 
				request_arbitrator.push_request(i);

		uint request_index = request_arbitrator.pop_request();
		if(request_index != ~0u)
		{
			//we have a request for a new scene segment this means we can decrement buckets in flight for the scene sgment whose bucket was previouly in the ray staging buffer
			MemoryRequestItem request = request_bus.get_data(request_index);

			if(request.type == MemoryRequestItem::Type::STORE)
			{
				//TODO figure out how to enqueue ray_writes
				//maybe the line address can encode ray ID and target bucket and in that way we can coalesc the rays or we can send a full ray at a time from the staging buffer like a cache line transfer
			}
			else if(request.type == MemoryRequestItem::Type::LOAD)
			{
				request_bus.clear_pending(request_index);

				uint last_segment_index = request.data_u32;
				if(last_segment_index != ~0u)
				{
					SceneSegmentState& last_segment_state = segment_states[last_segment_index];
					last_segment_state.buckets_in_flight--; //we have one fewer buckets in flight for this segment

					if(last_segment_state.buckets_in_flight == 0 && segment_states[last_segment_index].drained)
					{
						//add child segments to eligable list since we can now intersect them
						for(uint i = 0; i < last_segment_state.num_children; ++i)
							eligable_segments.push(last_segment_state.child_index + i);

						//remove from active segments
						for(uint i = 0; i < active_segments.size(); ++i)
							if(active_segments[i] == last_segment_index)
							{
								active_segments.erase(active_segments.begin() + i);
								break;
							}

						//clear segment state
						segment_states.erase(last_segment_index);
					}
				}

				RayReadQueueEntry entry;
				entry.tm = request_index;
				entry.last_segment = last_segment_index;
				ray_read_queue.push(entry);
			}
		}
	}

	void forward_load_returns()
	{
		//forward load returns to the correct unit
		if(has_segment_return && !return_bus.get_pending(0))
		{
			has_segment_return = false;
			return_bus.set_data(segment_return, 128);
			return_bus.set_pending(128);
		}

		if(has_bucket_return && !return_bus.get_pending(ray_bucket_stream.tm))
		{
			has_bucket_return = false;
			return_bus.set_data(bucket_return, ray_bucket_stream.tm);
			return_bus.set_pending(ray_bucket_stream.tm);
		}
	}

	void update_streams()
	{
		if(scene_stream.base_address)
		{
			if(!main_mem->return_bus.get_pending(0))
			{
				//advance stream
				if(scene_stream.offset < 64 * 1024)
				{
					MemoryRequestItem request;
					request.size = CACHE_LINE_SIZE;
					request.type = MemoryRequestItem::Type::LOAD;
					request.line_paddr = scene_stream.base_address + scene_stream.offset;

					main_mem->request_bus.set_data(request, 0);
					main_mem->request_bus.set_pending(0);

					scene_stream.offset += request.size;
				}
				else
				{
					scene_stream.base_address = 0ull;
					segment_states[scene_stream.scene_segment].in_buffer = true;
				}
			}
		}
		else if(active_segments.size() < 64 && eligable_segments.size() > 0)
		{
			//open new stream
			SceneSegmentState next_segment;
			uint next_index = eligable_segments.front(); eligable_segments.pop();
			next_segment.in_buffer = false; //set to true once we finish streaming the segment into the buffer
			next_segment.drained = false;
			next_segment.buckets_in_flight = 0;
			//TODO: need to add a header to the scene sgments that encodes the child segments so we can manage the set of eligable segments
			//next_segment.child_index = ;
			//next_segment.num_children = ;

			paddr_t segment_address = scene_segments + 64 * 1024 * next_index;
			//this data will pass through the scheduler in the first line of the segment
			main_mem->direct_read(&next_segment.child_index, 4, segment_address + 0);
			main_mem->direct_read(&next_segment.num_children, 4, segment_address + 4);

			scene_stream.base_address  = segment_address;
			scene_stream.offset        = 0;
			scene_stream.scene_segment = next_index;
		}

		if(ray_bucket_stream.base_address)
		{
			if(!main_mem->return_bus.get_pending(1))
			{
				//advance stream
				if(ray_bucket_stream.offset < 2 * 1024)
				{
					MemoryRequestItem request;
					request.size = CACHE_LINE_SIZE;
					request.type = MemoryRequestItem::Type::LOAD;
					request.line_paddr = ray_bucket_stream.base_address + ray_bucket_stream.offset;

					main_mem->request_bus.set_data(request, 1);
					main_mem->request_bus.set_pending(1);

					ray_bucket_stream.offset += request.size;
				}
				else
				{
					//we can now reuse the ray bucket for new rays
					free_buckets.push(ray_bucket_stream.base_address);
					ray_bucket_stream.base_address = 0ull;
				}
			}
		}
		else if(!ray_read_queue.empty())
		{
			//open new stream
			uint segment = ray_read_queue.front().last_segment;
			uint tm = ray_read_queue.front().tm;
			ray_read_queue.pop();

			paddr_t ray_bucket = first_bucket[segment];
			if(ray_bucket == 0ull)
			{
				//find the next segment that has a bucket ready
				for(uint i = 0; i < active_segments.size(); ++i)
				{
					segment = active_segments[i];
					if(segment_states[segment].in_buffer)
					{
						ray_bucket = first_bucket[segment];
						assert(ray_bucket != 0);
						break;
					}
				}
			}

			//start streaming next ray bucket
			if(ray_bucket != 0ull)
			{
				paddr_t next_bucket_addr = first_bucket[segment];
				main_mem->direct_read(&next_bucket_addr, 8, next_bucket_addr);
				first_bucket[segment] = next_bucket_addr;

				if(next_bucket_addr == 0)
				{
					//mark current segment drained since we drained all ray buckets
					segment_states[segment].drained = true;
				}

				ray_bucket_stream.base_address = ray_bucket;
				ray_bucket_stream.offset = 0;
				ray_bucket_stream.scene_segment = segment;
				ray_bucket_stream.tm = tm;
			}
		}

		if(ray_write_stream.base_address)
		{
			if(!main_mem->return_bus.get_pending(2))
			{
				//advance stream
				if(ray_write_stream.offset < 10 * 4)
				{
					MemoryRequestItem request;
					request.size = 4;
					request.type = MemoryRequestItem::Type::STORE;
					request.line_paddr = scene_stream.base_address + scene_stream.offset;

					main_mem->request_bus.set_data(request, 2);
					main_mem->request_bus.set_pending(2);

					ray_write_stream.offset += request.size;
				}
				else
				{
					scene_stream.base_address = 0ull;
					segment_states[scene_stream.scene_segment].in_buffer = true;
				}
			}
		}
		else if(!ray_write_queue.empty())
		{
			//open new stream
			uint segment = ray_write_queue.front().segment;
			paddr_t bucket_address = bucket_states[segment].address;
			
			if(bucket_states[segment].fill == 56)
			{
				if(!main_mem->return_bus.get_pending(2))
				{
					if(free_buckets.empty())
					{
						free_buckets.push(next_bucket);
						next_bucket += 2 * 1024;
					}

					paddr_t new_bucket_address = free_buckets.top();
					free_buckets.pop();

					bucket_states[segment].address = new_bucket_address;
					bucket_states[segment].fill = 0;

					//TODO write the new bucket address to the last bucket address
					MemoryRequestItem request;
					request.size = 8;
					request.type = MemoryRequestItem::Type::STORE;
					request.line_paddr = scene_stream.base_address + scene_stream.offset;

					main_mem->request_bus.set_data(request, 2);
					main_mem->request_bus.set_pending(2);

					bucket_address = new_bucket_address;
				}
			}
			
			if(bucket_states[segment].fill < 56)
			{
				ray_write_stream.base_address = bucket_address + 16 + sizeof(BucketRay) * bucket_states[segment].fill;
				ray_write_stream.offset = 0;
				bucket_states[segment].fill++;
			}
		}
	}

	UnitStreamScheduler(UnitMainMemoryBase* main_mem, uint num_tms, Simulator* simulator) : UnitMemoryBase(num_tms + 1, simulator), request_arbitrator(num_tms)
	{

	}

	void clock_rise() override
	{
		process_load_returns();
		process_requests();
	}

	void clock_fall() override
	{
		forward_load_returns();
		update_streams();
	}
};

}}}
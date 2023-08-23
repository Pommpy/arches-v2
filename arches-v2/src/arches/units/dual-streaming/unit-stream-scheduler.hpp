#pragma once 
#include "../../../stdafx.hpp"

#include "../unit-base.hpp"
#include "../unit-memory-base.hpp"
#include "../unit-main-memory-base.hpp"
#include "../unit-buffer.hpp"

#include "../../../../benchmarks/dual-streaming/src/ray.hpp"
#include "../../../../benchmarks/dual-streaming/src/treelet-bvh.hpp"

namespace Arches { namespace Units { namespace DualStreaming {

#define ROW_SIZE (8 * 1024)
#define SCENE_BUFFER_SIZE (4 * 1024 * 1024)

#define MAX_ACTIVE_SEGMENTS (SCENE_BUFFER_SIZE / TREELET_SIZE)
#define ROWS_PER_SEGMENT (TREELET_SIZE / ROW_SIZE)

#define RAY_BUCKET_SIZE 2048
#define MAX_RAYS_PER_BUCKET (sizeof(BucketRay) / 2036)

struct RayBucket
{
	paddr_t next_bucket;
	uint num_rays{0};
	BucketRay bucket_rays[MAX_RAYS_PER_BUCKET];
};

class UnitStreamScheduler : public UnitMemoryBase
{
public:
	class Stream
	{
	public:
		paddr_t base_addr{0};
		uint size{0};
		uint interface_width{CACHE_BLOCK_SIZE};

		uint dst;

		uint num_requested{0};
		uint num_returned{0};

		Stream()
		{

		}

		Stream(paddr_t base_addr, uint bytes, uint interface_width, uint dst)
		{
			this->base_addr = base_addr;
			this->size = bytes / interface_width;
			this->interface_width = interface_width;
			this->dst = dst;
		}

		bool fully_requested()
		{
			return num_requested == size;
		}

		bool fully_returned()
		{
			return num_returned == size;
		}

		void count_return()
		{
			num_returned++;
		}

		MemoryRequest get_next_request()
		{
			MemoryRequest req;
			req.type = MemoryRequest::Type::LOAD;
			req.size = interface_width;
			req.paddr = base_addr + num_requested * interface_width;
			num_requested++;

			return req;
		}
	};

	struct Configuration
	{
		paddr_t scene_start;
		paddr_t bucket_start;

		uint num_tms;
		uint bucket_size;
		uint segment_size;
		uint ray_size;
	};

	struct RayWriteQueueEntry
	{
		BucketRay bucket_ray;
		uint segment_index;
	};

	struct RayReadQueueEntry
	{
		uint port_index;
		uint last_segment;
	};

	struct HitRecordWriteQueueEntry
	{
		paddr_t address;
		Hit hit;
		bool valid{false};
	};

	struct SceneSegmentState
	{
		uint index{0};
		uint buckets_in_flight{0};

		paddr_t head_bucket_address{0x0ull};

		uint child_index{~0u};
		uint num_children{0};

		uint rows_in_buffer{0};

		bool valid{false};
	};

	struct BucketList
	{
		paddr_t head_bucket_address;

		paddr_t tail_bucket_address;
		uint tail_fill;
		uint tail_channel;
	};

	std::queue<RayWriteQueueEntry> ray_write_queue;
	std::queue<RayReadQueueEntry> ray_read_queue;

	std::queue<paddr_t> hit_record_read_queue;
	std::map<paddr_t, Hit> new_hit_record_set;
	std::queue<paddr_t> hit_record_write_queue;

	std::stack<paddr_t>        free_buckets; //todo this should be a linked list in memory
	std::map<uint, BucketList> bucket_lists;

	std::queue<uint>               eligable_segments;
	std::vector<SceneSegmentState> active_segment_states;
	uint active_segment_arb_index;
	uint num_active_segments;

private:
	UnitMainMemoryBase* _main_mem;
	UnitBuffer* _scene_buffer;

	uint _num_channels, _num_tms;
	paddr_t _scene_start, _hit_record_start, _bucket_start, _bucket_end;

	Stream scene_stream;
	MemoryRequest scene_buffer_request;

	Stream ray_stream;
	MemoryReturn ray_stream_return;

	Hit hit_reg;

public:
	void accept_returns()
	{
		auto& mmint = _main_mem->interconnect;
		const MemoryReturn* ret;

		if((ret = mmint.get_return(0)) && scene_buffer_request.type == MemoryRequest::Type::NA)
		{
			scene_buffer_request.type = MemoryRequest::Type::STORE;
			scene_buffer_request.size = ret->size;
			scene_buffer_request.paddr = (scene_stream.dst * TREELET_SIZE) + (ret->paddr - scene_stream.base_addr);
			scene_buffer_request.data = ret->data;
			mmint.acknowlege_return(0);
		}

		if((ret = mmint.get_return(1)) && (ray_stream_return.type == MemoryReturn::Type::NA))
		{
			ray_stream_return = *ret;
			mmint.acknowlege_return(1);
		}

		if(ret = mmint.get_return(3))
		{
			//data returned for the ray queue validate the entry or squash it
			Hit current_hit = *(Hit*)ret->data;
			if(current_hit.t < new_hit_record_set[ret->paddr].t)
			{
				//further than current record squash
				new_hit_record_set.erase(ret->paddr);
			}
			else
			{
				//closer than current entry write back
				hit_record_write_queue.emplace(ret->paddr);
			}
			mmint.acknowlege_return(3);
		}
	}

	void accept_requests()
	{
		interconnect.propagate_requests([](const MemoryRequest& req, uint client, uint num_clients, uint num_servers)->uint
		{
			return 0;
		});

		uint port_index;
		const MemoryRequest* req = interconnect.get_request(0, port_index);
		if(!req) return;

		//we have a request for a new scene segment this means we can decrement buckets in flight for the scene sgment whose bucket was previouly in the ray staging buffer
		if(req->type == MemoryRequest::Type::SBRAY)
		{
			//forward the request to the queue
			ray_write_queue.push(*(RayWriteQueueEntry*)req->data);
			interconnect.acknowlege_request(0, port_index);
		}
		else if(req->type == MemoryRequest::Type::LOAD_RAY_BUCKET)
		{
			uint last_segment_index = ~0u;
			uint last_segment_buffer_index = *(uint32_t*)req->data;
			if(last_segment_buffer_index != ~0u)
			{
				SceneSegmentState& last_segment_state = active_segment_states[last_segment_buffer_index];
				last_segment_state.buckets_in_flight--; //we have one fewer buckets in flight for this segment

				if(!last_segment_state.head_bucket_address && last_segment_state.buckets_in_flight == 0) //we are done with the segment
				{
					//add child segments to eligable list since we can now intersect them
					for(uint i = 0; i < last_segment_state.num_children; ++i)
						eligable_segments.push(last_segment_state.child_index + i);

					//remove from active segments
					last_segment_state.valid = false;
					num_active_segments--;
				}

				last_segment_index = last_segment_state.index;
			}

			ray_read_queue.push({port_index, last_segment_index});
			interconnect.acknowlege_request(0, port_index);
		}
		else if(req->type == MemoryRequest::Type::CSHIT)
		{
			Hit hit = *(Hit*)req->data;

			if(new_hit_record_set.find(req->paddr) != new_hit_record_set.end())
			{
				if(hit.t < new_hit_record_set[req->paddr].t) new_hit_record_set[req->paddr] = hit;
				interconnect.acknowlege_request(0, port_index);
			}
			else
			{
				//todo set limit on the number of hit records in the set
				hit_record_read_queue.push(req->paddr);
				new_hit_record_set[req->paddr] = hit;
				interconnect.acknowlege_request(0, port_index);
			}
		}
	}

	void issue_requests()
	{
		auto& mmint = _main_mem->interconnect;

		if(!mmint.request_pending(0) && !scene_stream.fully_requested())
		{
			MemoryRequest req = scene_stream.get_next_request();
			_main_mem->interconnect.add_request(req, 0);
		}

		//forward the scene stream
		if(_scene_buffer->interconnect.request_pending(_num_tms) && (scene_buffer_request.type == MemoryRequest::Type::STORE))
		{
			_scene_buffer->interconnect.add_request(scene_buffer_request, _num_tms);
			scene_stream.num_returned++;
			scene_buffer_request.type = MemoryRequest::Type::NA;
		}

		if(!mmint.request_pending(1) && !ray_stream.fully_requested())
		{
			MemoryRequest req = ray_stream.get_next_request();
			_main_mem->interconnect.add_request(req, 1);
		}

		if(!mmint.request_pending(2) && !ray_write_queue.empty())
		{
			//write next ray
			uint segment_index = ray_write_queue.front().segment_index;
			BucketList& bucket_list = bucket_lists[segment_index];
			if(bucket_list.tail_fill == MAX_RAYS_PER_BUCKET)
			{
				//fetch a new bucket to append to the list
				if(free_buckets.empty())
				{
					free_buckets.push(_bucket_end);
					_bucket_end += RAY_BUCKET_SIZE;
				}
				paddr_t new_bucket_address = free_buckets.top(); free_buckets.pop();

				//link the new bucket to the tail bucket
				_main_mem->direct_write(&new_bucket_address, sizeof(paddr_t), bucket_list.tail_bucket_address);

				bucket_list.tail_bucket_address = new_bucket_address;
				bucket_list.tail_fill = 0;

				//zero the pointer for the new tail
				paddr_t zero = 0x0ull;
				uint max_fill = MAX_RAYS_PER_BUCKET;
				_main_mem->direct_write(&zero, sizeof(paddr_t), bucket_list.tail_bucket_address + 0); //Write null to next pointer in tail bucket
				_main_mem->direct_write(&max_fill, sizeof(uint), bucket_list.tail_bucket_address + sizeof(paddr_t)); //Write max fill premtivley. If it not fully filled when we pop this list we will fix it
			}

			MemoryRequest req;
			req.type = MemoryRequest::Type::STORE;
			req.size = sizeof(BucketRay);
			req.paddr = (paddr_t)(&(*(RayBucket*)&bucket_list.tail_bucket_address).bucket_rays[bucket_list.tail_fill]);
			req.data = &ray_write_queue.front().bucket_ray;
			mmint.add_request(req, 2);

			ray_write_queue.pop();
		}

		if(!mmint.request_pending(3) )
		{
			if(!hit_record_read_queue.empty())
			{
				MemoryRequest req;
				req.type = MemoryRequest::Type::LOAD;
				req.size = sizeof(Hit);
				req.paddr = hit_record_read_queue.front(); hit_record_read_queue.pop();
				mmint.add_request(req, 3);
			}
			else if(!hit_record_write_queue.empty())
			{
				MemoryRequest req;
				req.type = MemoryRequest::Type::STORE;
				req.size = sizeof(Hit);
				req.paddr = hit_record_write_queue.front(); hit_record_write_queue.pop();
				req.data = &hit_reg;
				hit_reg = new_hit_record_set[req.paddr];
				new_hit_record_set.erase(req.paddr);
				mmint.add_request(req, 3);
			}
		}
	}

	void issue_returns()
	{
		//forward the ray stream
		if(interconnect.return_pending(0) && (ray_stream_return.type == MemoryReturn::Type::LOAD_RETURN))
		{
			interconnect.add_return(ray_stream_return, 0, ray_stream.dst);
			ray_stream.num_returned++;
			ray_stream_return.type = MemoryReturn::Type::NA;
		}
	}

	uint select_segment_from_buffer(uint last_segment_index)
	{
		for(uint i = 0; i < active_segment_states.size(); ++i)
		{
			if(!active_segment_states[i].valid || !active_segment_states[i].head_bucket_address) continue;
			if(active_segment_states[i].index == last_segment_index) return i;
		}
		
		uint i = active_segment_arb_index;
		do
		{
			if(active_segment_states[i].valid && active_segment_states[i].head_bucket_address)
			{
				active_segment_arb_index = i;
				return i;
			}

			if(++i == active_segment_states.size()) i = 0;
		} 
		while(i != active_segment_arb_index);

		return ~0u;
	}

	void update_streams()
	{
		//if the current segment has no more pending streams we are ready for the next segment
		if(scene_stream.fully_returned() && eligable_segments.size() > 0 && num_active_segments < MAX_ACTIVE_SEGMENTS)
		{
			//queue up the next segment streams
			num_active_segments++;
			uint buffer_index = 0;
			for(buffer_index = 0; buffer_index < active_segment_states.size(); ++buffer_index)
				if(!active_segment_states[buffer_index].valid) break;

			SceneSegmentState& segment_state = active_segment_states[buffer_index];
			segment_state.index = eligable_segments.front(); eligable_segments.pop();
			segment_state.buckets_in_flight = 0;
			segment_state.rows_in_buffer = 0;
			segment_state.valid = true;
			segment_state.head_bucket_address = bucket_lists[segment_state.index].head_bucket_address;

			//write the last bucket fill to the last bucket
			_main_mem->direct_write(&bucket_lists[segment_state.index].tail_fill, sizeof(uint), bucket_lists[segment_state.index].tail_bucket_address + 8);
			bucket_lists.erase(segment_state.index);

			//this data will pass through the scheduler in the first line of the segment
			//TODO: need to add a header to the scene sgments that encodes the child segments so we can manage the set of eligable segments
			paddr_t segment_address = _scene_start + sizeof(Treelet) * segment_state.index;
			_main_mem->direct_read(&segment_state.child_index, 4, segment_address + 0);
			_main_mem->direct_read(&segment_state.num_children, 4, segment_address + 4);

			scene_stream = Stream(segment_address, TREELET_SIZE, CACHE_BLOCK_SIZE, buffer_index);
		}

		if(ray_stream.fully_returned() && !ray_read_queue.empty())
		{
			//open new stream
			uint segment_index = ray_read_queue.front().last_segment;
			uint port_index = ray_read_queue.front().port_index;
			uint buffer_index = select_segment_from_buffer(segment_index);
			if(buffer_index != ~0u)
			{
				ray_read_queue.pop();
				SceneSegmentState& segment_state = active_segment_states[buffer_index];

				//start streaming next ray bucket
				paddr_t next_bucket_addr = segment_state.head_bucket_address;
				_main_mem->direct_read(&segment_state.head_bucket_address, 8, next_bucket_addr); //this data will pass through as soon as the first request returns from the stream we are about to open

				ray_stream = Stream(next_bucket_addr, RAY_BUCKET_SIZE, CACHE_BLOCK_SIZE, port_index);
			}
		}
	}

	UnitStreamScheduler(Configuration config) :  UnitMemoryBase(config.num_tms, 1)
	{
		_scene_start = config.scene_start;
		_bucket_start = config.bucket_start;
		_bucket_end = _bucket_start;

		_num_channels = numDramChannels();
		_num_tms = config.num_tms;
	}

	void clock_rise() override
	{
		accept_requests();
		accept_returns();
	}

	void clock_fall() override
	{
		update_streams();
		issue_requests();
		issue_returns();
	}
};

}}}
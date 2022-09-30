#pragma once 
#include "../../stdafx.hpp"

#include "unit-base.hpp"
#include "unit-main-memory-base.hpp"

namespace Arches { namespace Units {

class UnitSRAM : public UnitMainMemoryBase
{
public:
	UnitSRAM(uint num_clients, uint64_t size, Simulator* simulator) : 
		UnitMainMemoryBase(num_clients, size, simulator), arbitrator(num_clients)
	{
	}

	RoundRobinArbitrator arbitrator;

	void clock_rise() override
	{
		for(uint i = 0; i < request_bus.size(); ++i)
			if(request_bus.get_pending(i)) arbitrator.push_request(i);

		for(uint i = 0; i < 16; ++i)
		{
			uint request_index;
			if((request_index = arbitrator.pop_request()) != ~0)
			{
				MemoryRequestItem* request_item = s.get_message(request_index);
				if(request_item->type == MemoryRequestItem::Type::STORE)
				{
					std::memcpy(&_data_u8[request_item->line_paddr + request_item->offset], &request_item->data[request_item->offset], request_item->size);
					//request_item->type = MemoryRequestItem::STORE_ACCEPT;
					//output_buffer.push_message(request_item, request_item->return_buffer_id, request_item->return_port);
					//just need to ackowlege request 
					//TODO acknowlege request
					acknowledge_buffer.push_message(_request_buffer.get_sending_unit(request_index), _request_buffer.id);
					_request_buffer.clear(request_index);
				}

				if(request_item->type == MemoryRequestItem::Type::LOAD)
				{
					std::memcpy(request_item->data, &_data_u8[request_item->line_paddr], CACHE_LINE_SIZE);
					request_item->type = MemoryRequestItem::Type::LOAD_RETURN;

					for(uint i = 0; i < request_item->return_buffer_id_stack_size; ++i)
						output_buffer.push_message(request_item, request_item->return_buffer_id_stack[i], 0);

					acknowledge_buffer.push_message(_request_buffer.get_sending_unit(request_index), _request_buffer.id);
					_request_buffer.clear(request_index);
				}
			}
			else break;
		}
	}
};

}}
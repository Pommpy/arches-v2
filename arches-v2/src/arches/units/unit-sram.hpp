#pragma once 
#include "../../stdafx.hpp"

#include "unit-base.hpp"
#include "unit-main-memory-base.hpp"

namespace Arches { namespace Units {

class UnitSRAM : public UnitMainMemoryBase
{
public:
	UnitSRAM(uint64_t size, Simulator* simulator) : UnitMainMemoryBase(size, simulator)
	{
		output_buffer.resize(16, sizeof(MemoryRequestItem));
		acknowledge_buffer.resize(16);
	}

	void execute() override
	{
		_request_buffer.rest_arbitrator_round_robin();
		for(uint i = 0; i < 16; ++i)
		{
			uint request_index;
			if((request_index = _request_buffer.get_next_index()) != ~0)
			{
				MemoryRequestItem* request_item = _request_buffer.get_message(request_index);
				if(request_item->type == MemoryRequestItem::Type::STORE)
				{
					std::memcpy(&_data_u8[request_item->paddr + request_item->offset], &request_item->data[request_item->offset], request_item->size);
					//request_item->type = MemoryRequestItem::STORE_ACCEPT;
					//output_buffer.push_message(request_item, request_item->return_buffer_id, request_item->return_port);
					//just need to ackowlege request 
					//TODO acknowlege request
					acknowledge_buffer.push_message(_request_buffer.get_sending_unit(request_index), _request_buffer.id);
					_request_buffer.clear(request_index);
				}

				if(request_item->type == MemoryRequestItem::Type::LOAD)
				{
					std::memcpy(request_item->data, &_data_u8[request_item->paddr], CACHE_LINE_SIZE);
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
#pragma once 
#include "../../stdafx.hpp"

#include "unit-base.hpp"
#include "unit-main-memory-base.hpp"

namespace Arches { namespace Units {

class UnitSRAM : public UnitMainMemoryBase
{
public:
	UnitSRAM(uint64_t size, uint32_t num_clients, Simulator* simulator) : UnitMainMemoryBase(size, num_clients, simulator)
	{
		output_buffer.resize(16, sizeof(MemoryRequestItem));
	}

	void execute() override
	{
		MemoryRequestItem* request_item;
		_request_buffer.rest_arbitrator_round_robin();
		for(uint i = 0; i < 16; ++i)
		{
			if((request_item = _request_buffer.get_next()) != nullptr)
			{
				if(request_item->type == MemoryRequestItem::STORE)
				{
					std::memcpy(&_data_u8[request_item->paddr], request_item->data, request_item->size);
					request_item->type = MemoryRequestItem::STORE_ACCEPT;
					output_buffer.push_message(request_item, request_item->return_buffer_id, request_item->return_port);
				}

				if(request_item->type == MemoryRequestItem::LOAD)
				{
					std::memcpy(request_item->data, &_data_u8[request_item->paddr], request_item->size);
					request_item->type = MemoryRequestItem::LOAD_RETURN;
					output_buffer.push_message(request_item, request_item->return_buffer_id, request_item->return_port);
				}

				_request_buffer.clear_current();
			}
			else break;
		}
	}
};

}}
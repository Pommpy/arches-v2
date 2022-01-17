#pragma once 
#include "../../stdafx.hpp"

#include "unit-base.hpp"
#include "unit-memory-base.hpp"

namespace Arches { namespace Units {

class UnitAtomicIncrement : public UnitMemoryBase, public UnitBase
{
public:
	UnitAtomicIncrement(uint32_t num_clients, Simulator* simulator) : UnitMemoryBase(nullptr, num_clients, simulator), UnitBase(simulator)
	{
		output_buffer.resize(1, sizeof(MemoryRequestItem));
		executing = false;
	}

	uint32_t counter{0};

	void execute() override
	{
		MemoryRequestItem* request_item;
		_request_buffer.rest_arbitrator_round_robin();

		if(request_item = _request_buffer.get_next())
		{
			if(request_item->type == MemoryRequestItem::LOAD)
			{
				reinterpret_cast<uint32_t*>(request_item->data)[0] = counter;
				request_item->type = MemoryRequestItem::LOAD_RETURN;
				output_buffer.push_message(request_item, request_item->return_buffer_id, request_item->return_port);
				counter++;
			}

			_request_buffer.clear_current();
		}
	}
};

}}
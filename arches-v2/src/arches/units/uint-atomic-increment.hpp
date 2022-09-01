#pragma once 
#include "../../stdafx.hpp"

#include "unit-base.hpp"
#include "unit-memory-base.hpp"

namespace Arches { namespace Units {

class UnitAtomicIncrement : public UnitMemoryBase
{
public:
	UnitAtomicIncrement(Simulator* simulator) : UnitMemoryBase(nullptr, simulator)
	{
		output_buffer.resize(1, sizeof(MemoryRequestItem));
		acknowledge_buffer.resize(1);
		executing = false;
	}

	uint32_t counter{0};

	void execute() override
	{
		_request_buffer.rest_arbitrator_round_robin();

		uint request_index;
		if((request_index = _request_buffer.get_next_index()) != ~0)
		{
			MemoryRequestItem* request_item = _request_buffer.get_message(request_index);
			if(request_item->type == MemoryRequestItem::Type::LOAD)
			{
				if((counter & 0x0) == 0x0) printf(" Amoin: %d\r", counter);
				reinterpret_cast<uint32_t*>(request_item->data)[0] = counter++;
				request_item->type = MemoryRequestItem::Type::LOAD_RETURN;
				for(uint i = 0; i < request_item->return_buffer_id_stack_size; ++i)
					output_buffer.push_message(request_item, request_item->return_buffer_id_stack[i], 0);

				//acknowledge_buffer.push_message(_request_buffer.get_sending_unit(request_index), _request_buffer.id);
				_request_buffer.clear(request_index);
			}
		}
	}
};

}}
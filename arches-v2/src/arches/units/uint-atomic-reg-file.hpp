#pragma once 
#include "../../stdafx.hpp"

#include "unit-base.hpp"
#include "unit-memory-base.hpp"

namespace Arches { namespace Units {

class UnitAtomicRegfile : public UnitMemoryBase
{
public:
	uint32_t iregs[32];

	uint32_t return_reg;

	UnitAtomicRegfile(uint num_clients) : UnitMemoryBase(num_clients)
	{
		for(uint i = 0; i < 32; ++i)
			iregs[i] = 0;
	}

	uint request_index{~0u};
	MemoryRequest request_item;

	void clock_rise() override
	{
		if((request_index = request_bus.get_next()) != ~0)
		{
			request_item = request_bus.transfer(request_index);
			request_bus.acknowlege(request_index);
		}
	}

	void clock_fall() override
	{
		if(request_index != ~0)
		{
			uint32_t reg_index = (request_item.paddr >> 2) & 0b1'1111;
			uint32_t request_data = *(uint32_t*)request_item.data;
			return_reg = iregs[reg_index];
			request_item.data = &return_reg;

			switch(request_item.type)
			{
			case MemoryRequest::Type::STORE:
				iregs[reg_index] = request_data;
				break;

			case MemoryRequest::Type::LOAD:
				break;

			case MemoryRequest::Type::AMO_ADD:
				iregs[reg_index] += request_data;
				break;

			case MemoryRequest::Type::AMO_AND:
				iregs[reg_index] &= request_data;
				break;

			case MemoryRequest::Type::AMO_OR:
				iregs[reg_index] |= request_data;
				break;

			case MemoryRequest::Type::AMO_XOR:
				iregs[reg_index] ^= request_data;
				break;

			case MemoryRequest::Type::AMO_MIN:
				iregs[reg_index] = std::min((int32_t)request_data, (int32_t)iregs[reg_index]);
				break;

			case MemoryRequest::Type::AMO_MAX:
				iregs[reg_index] = std::max((int32_t)request_data, (int32_t)iregs[reg_index]);
				break;

			case MemoryRequest::Type::AMO_MINU:
				iregs[reg_index] = std::min(request_data, iregs[reg_index]);
				break;
			
			case MemoryRequest::Type::AMO_MAXU:
				iregs[reg_index] = std::max(request_data, iregs[reg_index]);
				break;
			}

			if(request_item.type != MemoryRequest::Type::STORE)
			{
				request_item.type = MemoryRequest::Type::LOAD_RETURN;
				return_bus.add_transfer(request_item, request_index);
			}
		}
	}
};

}}
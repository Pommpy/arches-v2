#pragma once 
#include "../../stdafx.hpp"

#include "unit-base.hpp"
#include "unit-memory-base.hpp"

#include "../util/arbitration.hpp"

namespace Arches { namespace Units {

class UnitAtomicRegfile : public UnitMemoryBase
{
public:
	uint32_t iregs[32];

	uint port_index{~0u};
	MemoryRequest current_request;

	CrossBar<MemoryRequest> _request_cross_bar;
	CrossBar<MemoryReturn> _return_cross_bar;

	UnitAtomicRegfile(uint num_clients) : UnitMemoryBase(),
		_request_cross_bar(num_clients, 1), _return_cross_bar(1, num_clients)
	{
		for(uint i = 0; i < 32; ++i)
			iregs[i] = 0;
	}

	void clock_rise() override
	{
		if(_request_cross_bar.is_read_valid(0))
		{
			current_request = _request_cross_bar.read(0, port_index);
		}
	}

	void clock_fall() override
	{
		if(port_index != ~0)
		{
			if(current_request.type != MemoryRequest::Type::STORE && !_return_cross_bar.is_write_valid(0)) return;

			uint32_t reg_index = (current_request.paddr >> 2) & 0b1'1111;
			uint32_t request_data = current_request.data_u32;
			uint32_t ret_val = iregs[reg_index];

			switch(current_request.type)
			{
			case MemoryRequest::Type::STORE:
				iregs[reg_index] = request_data;
				break;

			case MemoryRequest::Type::LOAD:
				break;

			case MemoryRequest::Type::AMO_ADD:
				iregs[reg_index] += request_data;
				//if(ret_val % 1024 == 0) 
					printf("Tiles Launched: %d\n", ret_val);
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

			if(current_request.type != MemoryRequest::Type::STORE)
			{
				MemoryReturn ret = current_request;
				ret.data_u32 = ret_val;
				_return_cross_bar.write(ret, 0, port_index);
			}

			port_index = ~0u;
		}
	}

	bool request_port_write_valid(uint port_index) override
	{
		return _request_cross_bar.is_write_valid(port_index);
	}

	void write_request(const MemoryRequest& request, uint port_index) override
	{
		_request_cross_bar.write(request, port_index, 0);
	}

	bool return_port_read_valid(uint port_index) override
	{
		return _return_cross_bar.is_read_valid(port_index);
	}

	const MemoryReturn& peek_return(uint port_index) override
	{
		return _return_cross_bar.peek(port_index);
	}

	const MemoryReturn& read_return(uint port_index) override
	{
		return _return_cross_bar.read(port_index);
	}
};

}}
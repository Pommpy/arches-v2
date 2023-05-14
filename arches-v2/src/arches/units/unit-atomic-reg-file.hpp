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
	MemoryRequest req;

	uint32_t return_reg;

	UnitAtomicRegfile(uint num_clients) : UnitMemoryBase(num_clients, 1)
	{
		for(uint i = 0; i < 32; ++i)
			iregs[i] = 0;
	}

	void clock_rise() override
	{
		interconnect.propagate_requests([](const MemoryRequest& req, uint client, uint num_clients, uint num_servers)->uint
		{
			//only one server so everything goes to index 0
			return 0;
		});

		const MemoryRequest* req = interconnect.get_request(0, port_index);
		if(req)
		{
			this->req = *req;
			interconnect.acknowlege_request(0, port_index);
		}
	}

	void clock_fall() override
	{
		interconnect.propagate_ack();

		if(port_index != ~0)
		{
			uint32_t reg_index = (req.paddr >> 2) & 0b1'1111;
			uint32_t request_data = *(uint32_t*)req.data;
			return_reg = iregs[reg_index];

			switch(req.type)
			{
			case MemoryRequest::Type::STORE:
				iregs[reg_index] = request_data;
				break;

			case MemoryRequest::Type::LOAD:
				break;

			case MemoryRequest::Type::AMO_ADD:
				iregs[reg_index] += request_data;
				printf("Threads Launched: %d\n", return_reg);
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

			if(req.type != MemoryRequest::Type::STORE && !interconnect.return_pending(0))
			{
				MemoryReturn ret;
				ret.type = MemoryReturn::Type::LOAD_RETURN;
				ret.size = req.size;
				ret.paddr = req.paddr;
				ret.data = &return_reg;
				interconnect.add_return(ret, 0, port_index);
			}
		}

		interconnect.propagate_returns();
	}
};

}}
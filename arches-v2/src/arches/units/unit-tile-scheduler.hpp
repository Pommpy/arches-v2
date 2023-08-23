#pragma once 
#include "../../stdafx.hpp"

#include "unit-base.hpp"
#include "unit-memory-base.hpp"
#include "unit-atomic-reg-file.hpp"
#include "../util/bit-manipulation.hpp"

namespace Arches { namespace Units {

class UnitThreadScheduler : public UnitMemoryBase
{
public:
	UnitThreadScheduler(uint num_tp, uint tm_index, UnitAtomicRegfile* atomic_regs) : UnitMemoryBase(num_tp, 1),
		num_tp(num_tp), tm_index(tm_index), atomic_regs(atomic_regs)
	{
		current_offset = num_tp;
	}

	UnitAtomicRegfile* atomic_regs;

	uint num_tp;
	uint tm_index;

	uint current_tile{0};
	uint current_offset{0};

	MemoryRequest current_req;
	uint current_port_index{~0u};

	uint32_t req_reg;
	uint32_t ret_reg;

	bool stalled_for_atomic_reg{false};

	void clock_rise() override
	{
		interconnect.propagate_requests([](const MemoryRequest& req, uint client, uint num_clients, uint num_servers)->uint
		{
			return 0;
		});

		if(stalled_for_atomic_reg)
		{
			const MemoryReturn* ret = atomic_regs->interconnect.get_return(tm_index);
			if(ret)
			{
				current_tile = *((uint32_t*)ret->data);
				current_offset = 0;

				stalled_for_atomic_reg = false;
				atomic_regs->interconnect.acknowlege_return(tm_index);
			}
			return;
		}

		if(current_port_index == ~0u)
		{
			uint port_index;
			const MemoryRequest* req = interconnect.get_request(0, port_index);
			if(!req) return;

			assert(req->type == MemoryRequest::Type::FCHTHRD);
			interconnect.acknowlege_request(0, port_index);
			current_req = *req;
			current_port_index = port_index;
		}
	}

	void clock_fall() override
	{
		interconnect.propagate_ack();

		if(stalled_for_atomic_reg || current_port_index == ~0u) return;

		if(current_offset == 64)
		{
			MemoryRequest req;
			req.type = MemoryRequest::Type::AMO_ADD;
			req.size = 4;
			req.paddr = 0x0ull;
			req.data = &req_reg;
			req_reg = 1;
			atomic_regs->interconnect.add_request(req, tm_index);
			stalled_for_atomic_reg = true;
		}
		else if(!interconnect.return_pending(0))
		{
			MemoryReturn ret;
			ret.type = MemoryReturn::Type::LOAD_RETURN;
			ret.size = 4;
			ret.paddr = current_req.paddr;
			ret.data = &ret_reg;

			//TODO: Tile
			uint x = (current_tile % (1024 / 8)) * 8 + (current_offset % 8);
			uint y = (current_tile / (1024 / 8)) * 8 + (current_offset / 8);
			ret_reg = y * 1024 + x;

			interconnect.add_return(ret, 0, current_port_index);

			current_offset++;
			current_port_index = ~0u;
		}

		interconnect.propagate_returns();
	}
};

}}
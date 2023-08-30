#pragma once 
#include "../../stdafx.hpp"

#include "unit-base.hpp"
#include "unit-memory-base.hpp"
#include "unit-atomic-reg-file.hpp"
#include "../util/bit-manipulation.hpp"

namespace Arches { namespace Units {

class UnitThreadScheduler : public UnitMemoryBase
{
private:
	CrossBar<MemoryRequest> _request_cross_bar;
	CrossBar<MemoryReturn> _return_cross_bar;

public:
	UnitThreadScheduler(uint num_tp, uint tm_index, UnitAtomicRegfile* atomic_regs) : UnitMemoryBase(),
		_request_cross_bar(num_tp, 1), _return_cross_bar(1, num_tp), num_tp(num_tp), tm_index(tm_index), atomic_regs(atomic_regs)
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

	bool stalled_for_atomic_reg{false};

	void clock_rise() override
	{
		if(stalled_for_atomic_reg)
		{
			if(atomic_regs->return_port_read_valid(tm_index))
			{
				const MemoryReturn& ret = atomic_regs->read_return(tm_index);
				current_tile = *((uint32_t*)ret.data);
				current_offset = 0;

				stalled_for_atomic_reg = false;
			}
			return;
		}

		if(current_port_index == ~0u)
		{
			if(!_request_cross_bar.is_read_valid(0)) return;
			current_req = _request_cross_bar.read(0, current_port_index);
			assert(current_req.type == MemoryRequest::Type::FCHTHRD);
		}
	}

	void clock_fall() override
	{
		if(stalled_for_atomic_reg || current_port_index == ~0u) return;

		if(current_offset == 64)
		{
			if(atomic_regs->request_port_write_valid(tm_index))
			{
				MemoryRequest req;
				req.type = MemoryRequest::Type::AMO_ADD;
				req.size = 4;
				req.paddr = 0x0ull;
				req.data_u32 = 1;
				atomic_regs->write_request(req, tm_index);
				stalled_for_atomic_reg = true;
			}
		}
		else if(_return_cross_bar.is_write_valid(0))
		{
			MemoryReturn ret = current_req;
			uint x = (current_tile % (1024 / 8)) * 8 + (current_offset % 8);
			uint y = (current_tile / (1024 / 8)) * 8 + (current_offset / 8);
			ret.data_u32 = y * 1024 + x;

			_return_cross_bar.write(ret, 0, current_port_index);

			current_offset++;
			current_port_index = ~0u;
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
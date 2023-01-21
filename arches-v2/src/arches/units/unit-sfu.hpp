#pragma once 
#include "../../stdafx.hpp"

#include "unit-base.hpp"
#include "../isa/execution-base.hpp"

namespace Arches { namespace Units {

class UnitSFU : public UnitBase
{
public:
	struct Request
	{
		ISA::RISCV::RegAddr dst;
	};

public:
	UnitSFU(uint issue_width, uint issue_rate, uint latency, uint num_clients, Simulator* simulator) :
		UnitBase(simulator), request_bus(num_clients), return_bus(num_clients),
		 issue_width(issue_width), issue_rate(issue_rate), latency(latency), _arbitrator(num_clients)
	{
		executing = false;
		issue_counters.resize(issue_width, 0);
	}

	ConnectionGroup<Request> request_bus;
	ConnectionGroup<Request> return_bus;

	uint issue_width;
	uint issue_rate;
	uint latency;

	std::vector<uint> issue_counters;

private:
	RoundRobinArbitrator _arbitrator;

	cycles_t current_cycle{0};
	
	struct Return
	{
		Request  request;
		uint16_t index;
		cycles_t return_cycle;

		bool operator <(const Return& other) const { return return_cycle > other.return_cycle; }
	};

	std::priority_queue<Return> _return_queue;

	void clock_rise() override
	{
		for(uint i = 0; i < request_bus.size(); ++i)
			if(request_bus.get_pending(i)) _arbitrator.push_request(i);

		for(uint i = 0; i < issue_width; ++i)
		{
			if(issue_counters[i] == 0)
			{
				uint request_index = _arbitrator.pop_request();
				if(request_index == ~0u) break;

				Request request_item = request_bus.get_data(request_index);
				request_bus.clear_pending(request_index);

				_return_queue.push({request_item, static_cast<uint16_t>(request_index), current_cycle + std::max(1u, latency - 1u)});//simulate forwarding by returning a cycle eraly single cycle instruction will have to be handeled sepratly
				issue_counters[i] = issue_rate - 1;
			}
			else
			{
				issue_counters[i]--;
			}
		}
	}

	void clock_fall() override
	{
		while(!_return_queue.empty() && _return_queue.top().return_cycle <= current_cycle)
		{
			Return return_item = _return_queue.top(); _return_queue.pop();
			return_bus.set_data(return_item.request, return_item.index);
			return_bus.set_pending(return_item.index);
		}

		current_cycle++;
	}
};

}}
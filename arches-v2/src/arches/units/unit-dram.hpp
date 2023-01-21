#pragma once 
#include "../../stdafx.hpp"

#include "USIMM/memory_controller.h"

#include "unit-base.hpp"
#include "unit-main-memory-base.hpp"

namespace Arches { namespace Units {

class UnitDRAM : public UnitMainMemoryBase, public UsimmListener
{
private:
	struct _Request
	{
		MemoryRequest request;
		uint              bus_index;
	};
	std::unordered_map<uint32_t, _Request> _request_map;

	RoundRobinArbitrator arbitrator;

	struct ReturnItem
	{
		uint              request_id;
		cycles_t          return_cycle;

		ReturnItem(uint request_id, cycles_t return_cycle) : request_id(request_id), return_cycle(return_cycle) {}
	
		friend bool operator<(const ReturnItem& l, const ReturnItem& r)
		{
			return l.return_cycle > r.return_cycle;
		}
	};
	std::priority_queue<ReturnItem> _request_return_queue;

	uint32_t _next_request_id{0};
	cycles_t _current_cycle{ 0 };

public:
	UnitDRAM(uint num_clients, uint64_t size, Simulator* simulator);
	virtual ~UnitDRAM() override;

	void clock_rise() override;
	void clock_fall() override;

	bool usimm_busy();
	void print_usimm_stats(uint32_t const L2_line_size, uint32_t const word_size, cycles_t cycle_count);
	float total_power_in_watts();

	virtual void UsimmNotifyEvent(paddr_t const address, cycles_t write_cycle, uint32_t request_id);

private:
	bool _load(const MemoryRequest& request_item, uint request_index);
	bool _store(const MemoryRequest& request_item, uint request_index);
};

}}
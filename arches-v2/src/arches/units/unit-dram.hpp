#pragma once 
#include "../../stdafx.hpp"

#include "USIMM/memory_controller.h"

#include "unit-base.hpp"
#include "unit-main-memory-base.hpp"
#include "../util/round-robin-arbitrator.hpp"

namespace Arches { namespace Units {

class UnitDRAM : public UnitMainMemoryBase, public UsimmListener
{
private:
	struct ReturnItem
	{
		paddr_t	 paddr;
		uint	 port;
		cycles_t return_cycle;

		ReturnItem(paddr_t paddr, uint port, cycles_t return_cycle) : port(port), paddr(paddr), return_cycle(return_cycle) {}
	
		friend bool operator<(const ReturnItem& l, const ReturnItem& r)
		{
			return l.return_cycle > r.return_cycle;
		}
	};

	bool _busy{false};

	struct Channel
	{
		std::priority_queue<ReturnItem> request_return_queue;
		bool return_pending;
	};

	std::vector<Channel> _channels;

	ArbitrationNetwork incoming_request_network;
	ArbitrationNetwork outgoing_return_network;

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
#pragma once 
#include "../../stdafx.hpp"

#include "USIMM/memory_controller.h"

#include "unit-base.hpp"
#include "unit-main-memory-base.hpp"
#include "../util/arbitration.hpp"

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
		std::priority_queue<ReturnItem> return_queue;
	};

	std::vector<Channel> _channels;

	CrossBar<MemoryRequest> _request_cross_bar;
	CrossBar<MemoryReturn> _return_cross_bar;

	cycles_t _current_cycle{ 0 };

public:
	UnitDRAM(uint num_clients, uint64_t size, Simulator* simulator);
	virtual ~UnitDRAM() override;

	bool request_port_write_valid(uint port_index) override;
	void write_request(const MemoryRequest& request, uint port_index) override;

	bool return_port_read_valid(uint port_index) override;
	const MemoryReturn& peek_return(uint port_index) override;
	const MemoryReturn& read_return(uint port_index) override;

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
#pragma once 
#include "../../stdafx.hpp"

#include "USIMM/memory_controller.h"

#include "unit-base.hpp"
#include "unit-main-memory-base.hpp"

namespace Arches { namespace Units {

class UnitDRAM : public UnitMainMemoryBase, public UsimmListener
{
private:
	std::unordered_map<uint32_t, MemoryRequestItem> _request_map;

	struct ReturnItem
	{
		MemoryRequestItem request_item;
		cycles_t          return_cycle;

		ReturnItem(MemoryRequestItem request_item, cycles_t return_cycle) : request_item(request_item), return_cycle(return_cycle) {}
	
		friend bool operator<(const ReturnItem& l, const ReturnItem& r)
		{
			return l.return_cycle > r.return_cycle;
		}
	};
	std::priority_queue<ReturnItem> _request_return_queue;

	uint32_t _next_request_id;
	cycles_t _current_cycle{ 0 };

public:
	UnitDRAM(uint64_t size, Simulator* simulator);
	virtual ~UnitDRAM() override;

	void execute() override;

	bool usimm_busy();
	void print_usimm_stats(uint32_t const L2_line_size, uint32_t const word_size, cycles_t cycle_count);

	virtual void UsimmNotifyEvent(paddr_t const address, cycles_t write_cycle, uint32_t request_id);

private:
	void _load(MemoryRequestItem* request_item, uint request_index);
	void _store(MemoryRequestItem* request_item, uint request_index);
};

}}
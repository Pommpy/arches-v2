#include "unit-dram.hpp"

#include "USIMM/usimm.h"

namespace Arches { namespace Units {

UnitDRAM::UnitDRAM(uint64_t size, Simulator* simulator) : UnitMainMemoryBase(size, simulator)
{
	char* usimm_config_file = (char*)REL_PATH_BIN_TO_SAMPLES"gddr5_amd_map1_128col_8ch.cfg";
	char* usimm_vi_file = (char*)REL_PATH_BIN_TO_SAMPLES"1Gb_x16_amd2GHz.vi";

	if (usimm_setup(usimm_config_file, usimm_vi_file) < 0) assert(false); //usimm faild to initilize

	output_buffer.resize(16, sizeof(MemoryRequestItem));
	acknowledge_buffer.resize(16);
}

UnitDRAM::~UnitDRAM() /*override*/
{
	usimmDestroy();
}

bool UnitDRAM::usimm_busy() {
	return usimmIsBusy();
}

void UnitDRAM::print_usimm_stats(uint32_t const L2_line_size,
	uint32_t const word_size,
	cycles_t cycle_count)
{
	printUsimmStats(L2_line_size, word_size, cycle_count);
}

void UnitDRAM::UsimmNotifyEvent(paddr_t const address, cycles_t write_cycle, uint32_t request_id)
{
	MemoryRequestItem request_item = _request_map[request_id];
	_request_map.erase(request_id);

	assert((write_cycle - _current_cycle) >= 0);
	_request_return_queue.emplace(request_item, write_cycle);
}

void UnitDRAM::_load(MemoryRequestItem* request_item, uint request_index)
{
	//Note: in this method, we reuse `work_item` as the new load-return work item we send back down the memory hierarchy.
	paddr_t paddr = request_item->paddr;

	request_item->type = MemoryRequestItem::Type::LOAD_RETURN;
	std::memcpy(request_item->data, &_data_u8[request_item->paddr], CACHE_LINE_SIZE);

	//iterface with usimm
	dram_address_t const dram_addr = calcDramAddr(paddr);

	arches_request_t req;
	req.arches_addr = paddr;
	req.listener = this;
	req.id = _next_request_id;

	reqInsertRet_t request = insert_read(dram_addr, req, _current_cycle * DRAM_CLOCK_MULTIPLIER);

	if (request.retType == reqInsertRet_tt::RRT_READ_QUEUE_FULL) return;

	assert(request.retType == reqInsertRet_tt::RRT_WRITE_QUEUE || request.retType == reqInsertRet_tt::RRT_READ_QUEUE);

	if (request.retLatencyKnown)
	{
		//printf("%llu", request.completionTime / DRAM_CLOCK_MULTIPLIER - _current_cycle);
		_request_return_queue.emplace(*request_item, request.completionTime / DRAM_CLOCK_MULTIPLIER);
	}
	else
	{
		_request_map[req.id] = *request_item;
		++_next_request_id;
	}

	acknowledge_buffer.push_message(_request_buffer.get_sending_unit(request_index), _request_buffer.id);
	_request_buffer.clear(request_index);
}

void UnitDRAM::_store(MemoryRequestItem* request_item, uint request_index)
{
	paddr_t paddr = request_item->paddr;
	std::memcpy(&_data_u8[request_item->paddr + request_item->offset], &request_item->data[request_item->offset], request_item->size);

	//interface with usimm
	dram_address_t const dram_addr = calcDramAddr(paddr);

	arches_request_t req;
	req.arches_addr = paddr;
	req.listener = this;
	req.id = _next_request_id;

	reqInsertRet_t request = insert_write(dram_addr, req, _current_cycle * DRAM_CLOCK_MULTIPLIER);
	if (request.retType == reqInsertRet_tt::RRT_WRITE_QUEUE_FULL) return;

	assert(!request.retLatencyKnown);
	assert(request.retType == reqInsertRet_tt::RRT_WRITE_QUEUE);

	acknowledge_buffer.push_message(_request_buffer.get_sending_unit(request_index), _request_buffer.id);
	_request_buffer.clear(request_index);
}

void UnitDRAM::execute()
{
	uint request_index;
	_request_buffer.rest_arbitrator_round_robin();
	while((request_index = _request_buffer.get_next_index()) != ~0)
	{
		MemoryRequestItem* request_item = _request_buffer.get_message(request_index);
		switch (request_item->type)
		{
		case MemoryRequestItem::Type::STORE:
			_store(request_item, request_index);
			break;

		case MemoryRequestItem::Type::LOAD:
			_load(request_item, request_index);
			break;

		nodefault;
		}
	}

	++_current_cycle;
	for (uint i = 0; i < DRAM_CLOCK_MULTIPLIER; ++i)
		usimmClock();

	while (!_request_return_queue.empty() && _request_return_queue.top().return_cycle <= _current_cycle)
	{
		MemoryRequestItem request_item = _request_return_queue.top().request_item;
		for (uint i = 0; i < request_item.return_buffer_id_stack_size; ++i)
			output_buffer.push_message(&request_item, request_item.return_buffer_id_stack[i], 0);
		_request_return_queue.pop();
	}
}

}}
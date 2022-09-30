#include "unit-dram.hpp"

#include "USIMM/usimm.h"

namespace Arches { namespace Units {

UnitDRAM::UnitDRAM(uint num_clients, uint64_t size, Simulator* simulator) : 
	UnitMainMemoryBase(num_clients, size, simulator), arbitrator(num_clients)
{
	char* usimm_config_file = (char*)REL_PATH_BIN_TO_SAMPLES"gddr5_amd_map1_128col_8ch.cfg";
	char* usimm_vi_file = (char*)REL_PATH_BIN_TO_SAMPLES"1Gb_x16_amd2GHz.vi";

	if (usimm_setup(usimm_config_file, usimm_vi_file) < 0) assert(false); //usimm faild to initilize
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
	assert((write_cycle - _current_cycle) >= 0);
	_request_return_queue.emplace(request_id, write_cycle);
}

bool UnitDRAM::_load(const MemoryRequestItem& request_item, uint request_index)
{
	paddr_t line_paddr = request_item.line_paddr;

	//iterface with usimm
	dram_address_t const dram_addr = calcDramAddr(line_paddr);

	arches_request_t req;
	req.arches_addr = line_paddr;
	req.listener = this;
	req.id = _next_request_id;

	reqInsertRet_t request = insert_read(dram_addr, req, _current_cycle * DRAM_CLOCK_MULTIPLIER);

	if(request.retType == reqInsertRet_tt::RRT_READ_QUEUE_FULL) return false;

	assert(request.retType == reqInsertRet_tt::RRT_WRITE_QUEUE || request.retType == reqInsertRet_tt::RRT_READ_QUEUE);

	_request_map[_next_request_id].request = request_item;
	_request_map[_next_request_id].request.type = MemoryRequestItem::Type::LOAD_RETURN;
	std::memcpy(_request_map[_next_request_id].request.data, &_data_u8[line_paddr], CACHE_LINE_SIZE);
	_request_map[_next_request_id].bus_index = request_index;
	
	if (request.retLatencyKnown)
	{
		//printf("%llu", request.completionTime / DRAM_CLOCK_MULTIPLIER - _current_cycle);
		_request_return_queue.emplace(req.id, request.completionTime / DRAM_CLOCK_MULTIPLIER);
	}

	_next_request_id++;
	return true;
}

bool UnitDRAM::_store(const MemoryRequestItem& request_item, uint request_index)
{
	paddr_t paddr = request_item.line_paddr;
	std::memcpy(&_data_u8[request_item.line_paddr + request_item.offset], &request_item.data[request_item.offset], request_item.size);

	//interface with usimm
	dram_address_t const dram_addr = calcDramAddr(paddr);

	arches_request_t req;
	req.arches_addr = paddr;
	req.listener = this;
	req.id = _next_request_id;

	reqInsertRet_t request = insert_write(dram_addr, req, _current_cycle * DRAM_CLOCK_MULTIPLIER);
	if (request.retType == reqInsertRet_tt::RRT_WRITE_QUEUE_FULL) return false;

	assert(!request.retLatencyKnown);
	assert(request.retType == reqInsertRet_tt::RRT_WRITE_QUEUE);

	_next_request_id++;
	return true;
}

void UnitDRAM::clock_rise()
{
	for(uint i = 0; i < request_bus.size(); ++i)
		if(request_bus.get_pending(i)) arbitrator.push_request(i);

	uint request_index;
	while((request_index = arbitrator.pop_request()) != ~0)
	{
		const MemoryRequestItem& request = request_bus.get_bus_data(request_index);
		if(request.type == MemoryRequestItem::Type::STORE)
		{
			if(_store(request, request_index))
				request_bus.clear_pending(request_index);
			else break;
		}
		else if(request.type == MemoryRequestItem::Type::LOAD)
		{
			if(_load(request, request_index))
				request_bus.clear_pending(request_index);
			else break;
		}
	}
}

void UnitDRAM::clock_fall()
{
	for(uint i = 0; i < DRAM_CLOCK_MULTIPLIER; ++i)
		usimmClock();

	++_current_cycle;

	while(!_request_return_queue.empty() && _request_return_queue.top().return_cycle <= _current_cycle)
	{
		uint request_id = _request_return_queue.top().request_id;
		_request_return_queue.pop();

		auto it = _request_map.find(request_id);
		if(it != _request_map.end())
		{
			return_bus.set_bus_data(it->second.request, it->second.bus_index);
			return_bus.set_pending(it->second.bus_index);
			_request_map.erase(it);
		}
		else
		{
			printf("DRAM error couldn't find request_id: %d\n", request_id);
		}
	}

}

}}
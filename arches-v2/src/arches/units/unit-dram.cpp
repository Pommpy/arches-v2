#include "unit-dram.hpp"

#include "USIMM/usimm.h"

namespace Arches { namespace Units {

UnitDRAM::UnitDRAM(uint num_clients, uint64_t size, Simulator* simulator) : 
	UnitMainMemoryBase(num_clients, size)
{
	char* usimm_config_file = (char*)REL_PATH_BIN_TO_SAMPLES"gddr5.cfg";
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

float UnitDRAM::total_power_in_watts()
{
	return getUsimmPower() / 1000.0f;
}

void UnitDRAM::UsimmNotifyEvent(paddr_t const address, cycles_t write_cycle, uint32_t request_id)
{

	_request_return_queue.emplace(request_id, write_cycle);
	assert((write_cycle - _current_cycle) >= 0);
}

bool UnitDRAM::_load(const MemoryRequest& request_item, uint request_index)
{
	//iterface with usimm
	dram_address_t const dram_addr = calcDramAddr(request_item.paddr);

	arches_request_t req;
	req.arches_addr = request_item.paddr;
	req.listener = this;
	req.id = _next_request_id;

	reqInsertRet_t request = insert_read(dram_addr, req, _current_cycle * DRAM_CLOCK_MULTIPLIER);

	if(request.retType == reqInsertRet_tt::RRT_READ_QUEUE_FULL) return false;

	assert(request.retType == reqInsertRet_tt::RRT_WRITE_QUEUE || request.retType == reqInsertRet_tt::RRT_READ_QUEUE);

	_request_map[_next_request_id].request = request_item;
	_request_map[_next_request_id].request.type = MemoryRequest::Type::LOAD_RETURN;
	_request_map[_next_request_id].request.data = &_data_u8[request_item.paddr];
	_request_map[_next_request_id].bus_index = request_index;
	
	if (request.retLatencyKnown)
	{
		//printf("%llu", request.completionTime / DRAM_CLOCK_MULTIPLIER - _current_cycle);
		_request_return_queue.emplace(req.id, request.completionTime / DRAM_CLOCK_MULTIPLIER);
	}

	//printf("Load(%d): %lld(%d, %d, %d, %lld, %d)\n", req.id, request_item.paddr, dram_addr.channel, dram_addr.rank, dram_addr.bank, dram_addr.row, dram_addr.column);

	_next_request_id++;
	return true;
}

bool UnitDRAM::_store(const MemoryRequest& request_item, uint request_index)
{
	//interface with usimm
	dram_address_t const dram_addr = calcDramAddr(request_item.paddr);

	arches_request_t req;
	req.arches_addr = request_item.paddr;
	req.listener = this;
	req.id = _next_request_id;

	reqInsertRet_t request = insert_write(dram_addr, req, _current_cycle * DRAM_CLOCK_MULTIPLIER);
	if (request.retType == reqInsertRet_tt::RRT_WRITE_QUEUE_FULL) return false;

	assert(!request.retLatencyKnown);
	assert(request.retType == reqInsertRet_tt::RRT_WRITE_QUEUE);

	//printf("Store(%d): %lld(%d, %d, %d, %lld, %d)\n", req.id, request_item.paddr, dram_addr.channel, dram_addr.rank, dram_addr.bank, dram_addr.row, dram_addr.column);

	std::memcpy(&_data_u8[request_item.paddr], request_item.data, request_item.size);

	_next_request_id++;
	return true;
}

void UnitDRAM::clock_rise()
{
	uint request_priority_index = request_bus.get_next_pending();
	if(request_priority_index == ~0u) return;

	for(uint i = 0; i < request_bus.size(); ++i)
	{
		uint request_index = (request_priority_index + i) % request_bus.size();
		if(!request_bus.get_pending(request_index)) continue;

		const MemoryRequest& request = request_bus.get_data(request_index);
		if(request.type == MemoryRequest::Type::STORE)
		{
			if(_store(request, request_index))
				request_bus.clear_pending(request_index);
		}
		else if(request.type == MemoryRequest::Type::LOAD)
		{
			if(_load(request, request_index))
				request_bus.clear_pending(request_index);
		}

		if(!_busy)
		{
			_busy = true;
			simulator->units_executing++;
		}
	}
}

void UnitDRAM::clock_fall()
{
	for(uint i = 0; i < DRAM_CLOCK_MULTIPLIER; ++i)
		usimmClock();

	++_current_cycle;

	if(!_request_return_queue.empty() && _current_cycle >= (_request_return_queue.top().return_cycle))
	{
		uint request_id = _request_return_queue.top().request_id;
		auto it = _request_map.find(request_id);
		assert(it != _request_map.end());

		if(!return_bus.get_pending(it->second.bus_index))
		{
			return_bus.set_data(it->second.request, it->second.bus_index);
			return_bus.set_pending(it->second.bus_index);
			_request_return_queue.pop();
			_request_map.erase(it);
		}
	}

	if(_busy && !usimmIsBusy())
	{
		_busy = false;
		simulator->units_executing--;
	}
}

}}
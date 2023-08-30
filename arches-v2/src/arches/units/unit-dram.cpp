#include "unit-dram.hpp"

#include "USIMM/usimm.h"

namespace Arches { namespace Units {

UnitDRAM::UnitDRAM(uint num_ports, uint64_t size, Simulator* simulator) : UnitMainMemoryBase(size),
	_request_cross_bar(num_ports, 16), _return_cross_bar(16, num_ports)
{
	char* usimm_config_file = (char*)REL_PATH_BIN_TO_SAMPLES"gddr5.cfg";
	char* usimm_vi_file = (char*)REL_PATH_BIN_TO_SAMPLES"1Gb_x16_amd2GHz.vi";
	if (usimm_setup(usimm_config_file, usimm_vi_file) < 0) assert(false); //usimm faild to initilize

	_channels.resize(numDramChannels());
}

UnitDRAM::~UnitDRAM() /*override*/
{
	usimmDestroy();
}

bool UnitDRAM::request_port_write_valid(uint port_index)
{
	return _request_cross_bar.is_write_valid(port_index);
}

void UnitDRAM::write_request(const MemoryRequest& request, uint port_index)
{
	uint channel_index = calcDramAddr(request.paddr).channel;
	_request_cross_bar.write(request, port_index, channel_index);
}

bool UnitDRAM::return_port_read_valid(uint port_index)
{
	return _return_cross_bar.is_read_valid(port_index);
}

const MemoryReturn& UnitDRAM::peek_return(uint port_index)
{
	return _return_cross_bar.peek(port_index);
}

const MemoryReturn& UnitDRAM::read_return(uint port_index)
{
	return _return_cross_bar.read(port_index);
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
	dram_address_t const dram_addr = calcDramAddr(address);
	_channels[dram_addr.channel].return_queue.emplace(address, request_id, write_cycle);
}

bool UnitDRAM::_load(const MemoryRequest& request_item, uint port_index)
{
	//iterface with usimm
	dram_address_t const dram_addr = calcDramAddr(request_item.paddr);

	arches_request_t req;
	req.arches_addr = request_item.paddr;
	req.listener = this;
	req.id = port_index;

	reqInsertRet_t request = insert_read(dram_addr, req, _current_cycle * DRAM_CLOCK_MULTIPLIER);

	if(request.retType == reqInsertRet_tt::RRT_READ_QUEUE_FULL) return false;

	assert(request.retType == reqInsertRet_tt::RRT_WRITE_QUEUE || request.retType == reqInsertRet_tt::RRT_READ_QUEUE);

	if (request.retLatencyKnown)
	{
		_channels[dram_addr.channel].return_queue.emplace(request_item.paddr, port_index, request.completionTime / DRAM_CLOCK_MULTIPLIER);
	}

	//printf("Load(%d): %lld(%d, %d, %d, %lld, %d)\n", req.id, request_item.paddr, dram_addr.channel, dram_addr.rank, dram_addr.bank, dram_addr.row, dram_addr.column);
	return true;
}

bool UnitDRAM::_store(const MemoryRequest& request_item, uint port_index)
{
	//interface with usimm
	dram_address_t const dram_addr = calcDramAddr(request_item.paddr);

	arches_request_t req;
	req.arches_addr = request_item.paddr;
	req.listener = this;
	req.id = port_index;

	reqInsertRet_t request = insert_write(dram_addr, req, _current_cycle * DRAM_CLOCK_MULTIPLIER);
	if (request.retType == reqInsertRet_tt::RRT_WRITE_QUEUE_FULL) return false;

	assert(!request.retLatencyKnown);
	assert(request.retType == reqInsertRet_tt::RRT_WRITE_QUEUE);

	//printf("Store(%d): %lld (%d, %d, %d, %lld, %d)\n", req.id, request_item.paddr, dram_addr.channel, dram_addr.rank, dram_addr.bank, dram_addr.row, dram_addr.column);
	std::memcpy(&_data_u8[request_item.paddr], request_item.data, request_item.size);

	return true;
}

void UnitDRAM::clock_rise()
{
	for(uint channel_index = 0; channel_index < _channels.size(); ++channel_index)
	{
		if(!_request_cross_bar.is_read_valid(channel_index)) continue;

		uint port_index;
		const MemoryRequest& request = _request_cross_bar.peek(channel_index, port_index);

		if(request.type == MemoryRequest::Type::STORE)
		{
			if(_store(request, port_index))
				_request_cross_bar.read(channel_index, port_index);
		}
		else if(request.type == MemoryRequest::Type::LOAD)
		{
			if(_load(request, port_index))
				_request_cross_bar.read(channel_index, port_index);
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

	if(_busy && !usimmIsBusy())
	{
		_busy = false;
		simulator->units_executing--;
	}

	++_current_cycle;
	for(uint channel_index = 0; channel_index < _channels.size(); ++channel_index)
	{
		Channel& channel = _channels[channel_index];
		if(_return_cross_bar.is_write_valid(channel_index) && !channel.return_queue.empty())
		{
			const ReturnItem& return_item = channel.return_queue.top();
			if(_current_cycle >= return_item.return_cycle)
			{
				MemoryReturn ret;
				ret.type = MemoryReturn::Type::LOAD_RETURN;
				ret.size = CACHE_BLOCK_SIZE;
				ret.paddr = return_item.paddr;
				std::memcpy(ret.data, &_data_u8[return_item.paddr], CACHE_BLOCK_SIZE);
				_return_cross_bar.write(ret, channel_index, return_item.port);
				channel.return_queue.pop();
				//printf("Load Return(%d): %lld\n", return_item.port, return_item.paddr);
			}
		}
	}
}



}}
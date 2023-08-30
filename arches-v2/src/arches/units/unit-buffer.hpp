#pragma once 
#include "../../stdafx.hpp"

#include "../util/bit-manipulation.hpp"
#include "unit-base.hpp"
#include "unit-memory-base.hpp"

namespace Arches { namespace Units {

class UnitBuffer : public UnitMemoryBase
{
public:
	struct Configuration
	{
		uint64_t size{1024};
		uint port_width{CACHE_BLOCK_SIZE};
		uint num_banks{1};
		uint num_ports{1};
		uint penalty{1};

		float dynamic_read_energy;
		float bank_leakge_power;
	};

private:
	struct _Bank
	{
		uint cycles_remaining{0u};

		uint64_t request_mask{0x0ull};
		MemoryRequest request;

		uint8_t return_reg[CACHE_BLOCK_SIZE];
	};

	Configuration _configuration; //nice for debugging

	uint _bank_index_offset;
	uint64_t _buffer_address_mask, _bank_index_mask;

	uint _penalty, _port_width;

	uint8_t* data_u8;
	std::vector<_Bank> _banks;

	CrossBar<MemoryRequest> _request_cross_bar;
	CrossBar<MemoryReturn> _return_cross_bar;

public:
	UnitBuffer(Configuration config) : UnitMemoryBase(),
		_request_cross_bar(config.num_ports, config.num_banks), _return_cross_bar(config.num_banks, config.num_ports)
	{
		data_u8 = (uint8_t*)malloc(config.size);

		_port_width = config.port_width;
		_bank_index_mask = generate_nbit_mask(log2i(config.num_banks));
		_bank_index_offset = log2i(config.port_width); //stride in terms of port width

		_buffer_address_mask = generate_nbit_mask(log2i(config.size));

		_penalty = config.penalty;
	}

	~UnitBuffer()
	{
		free(data_u8);
	}

	void clock_rise() override
	{
		//select next request
		for(uint bank_index = 0; bank_index < _banks.size(); ++bank_index)
		{
			_Bank& bank = _banks[bank_index];
			if(bank.request_mask) continue;

			if(!_request_cross_bar.is_read_valid(bank_index)) continue;
			uint port_index;
			bank.request = _request_cross_bar.read(bank_index, port_index);
			bank.request_mask |= 0x1ull << port_index;
		}
	}

	void clock_fall() override
	{
		for(uint bank_index = 0; bank_index < _banks.size(); ++bank_index)
		{
			_Bank& bank = _banks[bank_index];
			if(bank.cycles_remaining == 0 || --bank.cycles_remaining != 0) continue;

			paddr_t buffer_addr = _get_buffer_addr(bank.request.paddr);
			if(bank.request.type == MemoryRequest::Type::LOAD)
			{
				if(!_return_cross_bar.is_write_valid(bank_index))
				{
					//cant return because of network trafic we must stall and try again next cycle
					bank.cycles_remaining = 1;
					continue;
				}

				MemoryReturn ret;
				ret.type = MemoryReturn::Type::LOAD_RETURN;
				ret.size = bank.request.size;
				ret.paddr = bank.request.paddr;
				std::memcpy(ret.data, &data_u8[buffer_addr], bank.request.size);

				_return_cross_bar.broadcast(ret, bank_index, bank.request_mask);
				bank.request_mask = 0x0ull;//we are ready for the next request
			}
			else if(bank.request.type == MemoryRequest::Type::STORE)
			{
				std::memcpy(&data_u8[buffer_addr], bank.request.data, bank.request.size);
				bank.request_mask = 0x0ull;
			}
		}
	}

	bool request_port_write_valid(uint port_index)
	{
		return _request_cross_bar.is_write_valid(port_index);
	}

	void write_request(const MemoryRequest& request, uint port_index)
	{
		uint bank_index = _get_bank_index(request.paddr);
		_request_cross_bar.write(request, port_index, bank_index);
	}

	bool return_port_read_valid(uint port_index)
	{
		return _return_cross_bar.is_read_valid(port_index);
	}

	const MemoryReturn& peek_return(uint port_index)
	{
		return _return_cross_bar.peek(port_index);
	}

	const MemoryReturn& read_return(uint port_index)
	{
		return _return_cross_bar.read(port_index);
	}

private:
	paddr_t _get_bank_index(paddr_t paddr) { return (paddr >> _bank_index_offset) & _bank_index_mask; }
	paddr_t _get_buffer_addr(paddr_t paddr) { return paddr & _buffer_address_mask; }
};

}}
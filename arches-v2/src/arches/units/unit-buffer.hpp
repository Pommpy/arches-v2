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
		uint num_incoming_connections{1};
		uint penalty{1};

		float dynamic_read_energy;
		float bank_leakge_power;
	};

private:
	struct _Bank
	{
		RoundRobinArbitrator64 arbitrator;

		uint cycles_remaining{0u};
		bool busy{false};

		MemoryRequest request;
		uint64_t port_mask{0x0ull};
	};

	Configuration _configuration; //nice for debugging

	uint _bank_priority_index{0};
	uint _port_priority_index{0};

	uint _bank_index_offset;
	uint64_t _buffer_address_mask, _bank_index_mask;

	uint _penalty;
	uint8_t* data_u8;

	std::vector<_Bank> _banks;
	uint _num_incoming_connections;

public:
	UnitBuffer(Configuration config) : UnitMemoryBase(config.num_incoming_connections)
	{
		data_u8 = (uint8_t*)malloc(config.size);

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
		//add requests to bank
		for(uint port_index = 0; port_index < request_bus.size(); ++port_index)
		{
			if(!request_bus.transfer_pending(port_index)) continue;

			const MemoryRequest& request = request_bus.transfer(port_index);
			uint32_t bank_index = _get_bank_index(request.paddr);
			_banks[bank_index].arbitrator.add(port_index);
		}
	}

	void clock_fall() override
	{
		for(uint i = 0; i < _banks.size(); ++i) _update_bank(i);
	}

private:
	void _update_bank(uint bank_index)
	{
		_Bank& bank = _banks[bank_index];

		//propagate ack

		bank.arbitrator.update();

		if(bank.arbitrator.current() == ~0u) return;

		//return data
	}

	paddr_t _get_bank_index(paddr_t paddr) { return (paddr >> _bank_index_offset) & _bank_index_mask; }
	paddr_t _get_buffer_addr(paddr_t paddr) { return paddr & _buffer_address_mask; }
};

}}
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
		uint bank_stride{CACHE_BLOCK_SIZE};
		uint num_banks{1};
		uint num_incoming_connections{1};
		uint penalty{1};

		float dynamic_read_energy;
		float bank_leakge_power;
	};

private:
	struct _Bank
	{
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
		executing = false;
		data_u8 = (uint8_t*)malloc(config.size);

		_bank_index_mask = generate_nbit_mask(log2i(config.num_banks));
		_bank_index_offset = log2i(config.bank_stride); //stride in terms of port width

		_buffer_address_mask = generate_nbit_mask(log2i(config.size));

		_penalty = config.penalty;
	}

	~UnitBuffer()
	{
		free(data_u8);
	}

	void clock_rise() override
	{
		_proccess_requests();
	}

	void clock_fall() override
	{
		for(uint i = 0; i < _banks.size(); ++i)
		{
			uint bank_index = (_bank_priority_index + i) % _banks.size();
			_update_bank(bank_index);
		}
		_bank_priority_index = (_bank_priority_index + 1) % _banks.size();
	}

private:
	void _proccess_requests()
	{
		for(uint i = 0; i < _num_incoming_connections; ++i)
		{
			uint request_index = (_port_priority_index + i) % _num_incoming_connections;
			if(!request_bus.transfer_pending(request_index)) continue;

			const MemoryRequest& request = request_bus.transfer(request_index);
			uint32_t bank_index = _get_bank(request.paddr);
			paddr_t buffer_addr = _get_buffer_addr(request.paddr);
			_Bank& bank = _banks[bank_index];

			if(bank.request.type == MemoryRequest::Type::STORE)
			{
				if(!bank.busy)
				{
					bank.cycles_remaining = _penalty;
					bank.busy = true;

					std::memcpy(&data_u8[buffer_addr], bank.request.data, bank.request.size);
				}
			}
			else if(bank.request.type == MemoryRequest::Type::LOAD)
			{
				if(!bank.busy)
				{
					//if bank isn't busy and there are no pending requests to that bank then we can skip it
					bank.cycles_remaining = _penalty;
					bank.busy = true;

					bank.request = request;
					bank.port_mask = 0x1ull << request_index;

					request_bus.acknowlege(request_index);
				}
				else
				{
					if(bank.request.type == request.type && request.paddr == bank.request.paddr && request.size == bank.request.size)
					{
						//merge requests to the same address
						bank.port_mask |= 0x1ull << request_index;

						request_bus.acknowlege(request_index);
					}
					else; //bank conflict
				}
			}
		}
	}

	void _update_bank(uint bank_index)
	{
		_Bank& bank = _banks[bank_index];
		if(!bank.busy) return;

		if(bank.request.type == MemoryRequest::Type::STORE)
		{
			if(bank.cycles_remaining > 0) bank.cycles_remaining--;
			if(bank.cycles_remaining != 0) return;

			bank.busy = false;
		}
		else if(bank.request.type == MemoryRequest::Type::LOAD)
		{
			if(bank.cycles_remaining > 0) bank.cycles_remaining--;
			if(bank.cycles_remaining != 0) return;

			//Try to return data
			paddr_t buffer_addr = _get_buffer_addr(bank.request.paddr);
			bank.request.type = MemoryRequest::Type::LOAD_RETURN;
			bank.request.data = &data_u8[buffer_addr];
		}

		if(bank.request.type == MemoryRequest::Type::LOAD_RETURN)
		{
			//Try to return data for the line we just recived
			//If we succed to return to all clients we free the bank otherwise we will try again next cycle
			_try_return(bank.request, bank.port_mask);
			if(bank.port_mask == 0x0ull) bank.busy = false;
		}
	}

	void _try_return(const MemoryRequest& rtrn, uint64_t& mask)
	{
		assert(rtrn.type == MemoryRequest::Type::LOAD_RETURN);

		//return to all pending ports
		for(uint i = 0; i < return_bus.size(); ++i)
		{
			uint64_t imask = (0x1ull << i);
			if((mask & imask) == 0x0ull || return_bus.transfer_pending(i)) continue;

			return_bus.add_transfer(rtrn, i);
			return_bus.set_pending(i);

			mask &= ~imask;
		}
	}

	uint32_t _get_bank(paddr_t paddr) { return static_cast<uint32_t>((paddr >> _bank_index_offset) & _bank_index_mask); }
	paddr_t _get_buffer_addr(paddr_t paddr) { return paddr & _buffer_address_mask; }
};

}}
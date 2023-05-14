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

public:
	UnitBuffer(Configuration config) : UnitMemoryBase(config.num_ports, config.num_banks)
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
		interconnect.propagate_requests([](const MemoryRequest& req, uint client, uint num_clients, uint num_servers)->uint
		{
			return (req.paddr >> log2i(CACHE_BLOCK_SIZE)) % num_servers;
		});

		//select next request
		for(uint bank_index = 0; bank_index < _banks.size(); ++bank_index)
		{
			_Bank& bank = _banks[bank_index];
			if(bank.request_mask) continue;

			uint port_index;
			const MemoryRequest* req = interconnect.get_request(bank_index, port_index);
			if(!req) continue;

			//merge identical requests
			bank.request = *req;
			uint64_t request_mask = interconnect.merge_requests(bank.request);
			interconnect.acknowlege_requests(bank_index, request_mask);
			bank.request_mask |= request_mask;
		}
	}

	void clock_fall() override
	{
		interconnect.propagate_ack();

		for(uint bank_index = 0; bank_index < _banks.size(); ++bank_index)
		{
			_Bank& bank = _banks[bank_index];
			if(bank.cycles_remaining == 0 || --bank.cycles_remaining != 0) continue;

			paddr_t buffer_addr = _get_buffer_addr(bank.request.paddr);
			if(bank.request.type == MemoryRequest::Type::LOAD)
			{
				if(interconnect.return_pending(bank_index))
				{
					//cant return because of network trafic we must stall and try again next cycle
					bank.cycles_remaining = 1;
					continue;
				}

				MemoryReturn ret;
				ret.type = MemoryReturn::Type::LOAD_RETURN;
				ret.size = bank.request.size;
				ret.paddr = bank.request.paddr;

				std::memcpy(bank.return_reg, &data_u8[buffer_addr], bank.request.size);
				ret.data = bank.return_reg;

				interconnect.add_returns(ret, bank_index, bank.request_mask);
				bank.request_mask = 0x0ull;//we are ready for the next request
			}
			else if(bank.request.type == MemoryRequest::Type::STORE)
			{
				std::memcpy(&data_u8[buffer_addr], bank.request.data, bank.request.size);
				bank.request_mask = 0x0ull;
			}
		}

		interconnect.propagate_returns();
	}

private:
	paddr_t _get_bank_index(paddr_t paddr) { return (paddr >> _bank_index_offset) & _bank_index_mask; }
	paddr_t _get_buffer_addr(paddr_t paddr) { return paddr & _buffer_address_mask; }
};

}}
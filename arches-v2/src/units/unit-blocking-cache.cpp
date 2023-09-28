#include "unit-blocking-cache.hpp"

namespace Arches {namespace Units {

UnitBlockingCache::UnitBlockingCache(Configuration config) : 
	UnitCacheBase(config.size, config.associativity),
	_request_cross_bar(config.num_ports, config.num_banks, config.bank_select_mask),
	_return_cross_bar(config.num_ports, config.num_banks),
	_banks(config.num_banks, config.data_array_latency)
{
	_configuration = config;

	_mem_higher = config.mem_higher;
	_mem_higher_port_offset = config.mem_higher_port_offset;

	uint bank_index_bits = log2i(config.num_banks);
}

UnitBlockingCache::~UnitBlockingCache()
{

}

void UnitBlockingCache::_clock_rise(uint bank_index)
{
	_Bank& bank = _banks[bank_index];
	bank.data_array_pipline.clock();

	if(bank.state == _Bank::State::INVALID)
	{
		if(!_request_cross_bar.is_read_valid(bank_index)) return;
		bank.current_request = _request_cross_bar.read(bank_index);

		if(bank.current_request.type == MemoryRequest::Type::LOAD)
		{
			paddr_t block_addr = _get_block_addr(bank.current_request.paddr);
			uint block_offset = _get_block_offset(bank.current_request.paddr);
			_BlockData* block_data = _get_block(block_addr);

			log.log_tag_array_access();

			if(block_data)
			{
				MemoryReturn ret = bank.current_request;
				std::memcpy(ret.data, &block_data->bytes[block_offset], ret.size);
				bank.data_array_pipline.write(ret);
				bank.state = _Bank::State::INVALID;
				log.log_hit();
			}
			else
			{
				bank.state = _Bank::State::MISSED;
				log.log_miss();
			}
		}
		else if(bank.current_request.type == MemoryRequest::Type::STORE)
		{
			//stores go around
			bank.state = _Bank::State::MISSED;
			log.log_uncached_write();
		}
	}
	else if(bank.state == _Bank::State::ISSUED)
	{
		uint mem_higher_port_index = _mem_higher_port_offset + bank_index;
		if(!_mem_higher->return_port_read_valid(mem_higher_port_index)) return;

		const MemoryReturn ret = _mem_higher->read_return(mem_higher_port_index);
		assert(ret.paddr == _get_block_addr(ret.paddr));

		_insert_block(ret.paddr, ret.data);
		log.log_tag_array_access();
		log.log_data_array_write();

		uint block_offset = _get_block_offset(bank.current_request.paddr);
		std::memcpy(bank.current_request.data, &ret.data[block_offset], bank.current_request.size);

		bank.state = _Bank::State::FILLED;	
	}
}

void UnitBlockingCache::_clock_fall(uint bank_index)
{
	_Bank& bank = _banks[bank_index];
	if(bank.state == _Bank::State::MISSED)
	{
		uint mem_higher_port_index = _mem_higher_port_offset + bank_index;
		if(_mem_higher->request_port_write_valid(mem_higher_port_index))
		{
			if(bank.current_request.type == MemoryRequest::Type::LOAD)
			{
				MemoryRequest request = bank.current_request;
				request.paddr = _get_block_addr(bank.current_request.paddr);
				request.size = CACHE_BLOCK_SIZE;
				request.port = mem_higher_port_index;
				_mem_higher->write_request(request, request.port);
				bank.state = _Bank::State::ISSUED;
			}
			else if(bank.current_request.type == MemoryRequest::Type::STORE)
			{
				MemoryRequest request = bank.current_request;
				//req.paddr = _get_block_addr(bank.current_request.paddr);
				//req.write_mask = req.write_mask << _get_block_offset(bank.current_request.paddr);
				//req.size = CACHE_BLOCK_SIZE;
				request.port = mem_higher_port_index;
				_mem_higher->write_request(request, request.port);
				bank.state = _Bank::State::INVALID;
			}
		}
	}
	else if(bank.state == _Bank::State::FILLED)
	{
		if(bank.data_array_pipline.empty() && _return_cross_bar.is_write_valid(bank_index))
		{
			//early restart
			MemoryReturn ret = bank.current_request;
			std::memcpy(ret.data, bank.current_request.data, ret.size);
			_return_cross_bar.write(ret, bank_index);
			bank.state = _Bank::State::INVALID;
		}
	}

	if(bank.data_array_pipline.is_read_valid() && _return_cross_bar.is_write_valid(bank_index))
	{
		MemoryReturn ret = bank.data_array_pipline.read();
		_return_cross_bar.write(ret, bank_index);
	}
}


void UnitBlockingCache::clock_rise()
{
	_request_cross_bar.clock();

	for(uint i = 0; i < _banks.size(); ++i)
	{
		_clock_rise(i);
	}
}

void UnitBlockingCache::clock_fall()
{
	for(uint i = 0; i < _banks.size(); ++i)
	{
		_clock_fall(i);
	}

	_return_cross_bar.clock();
}

bool UnitBlockingCache::request_port_write_valid(uint port_index)
{
	return _request_cross_bar.is_write_valid(port_index);
}

void UnitBlockingCache::write_request(const MemoryRequest& request, uint port_index)
{
	_request_cross_bar.write(request, port_index);
}

bool UnitBlockingCache::return_port_read_valid(uint port_index)
{
	return _return_cross_bar.is_read_valid(port_index);
}

const MemoryReturn& UnitBlockingCache::peek_return(uint port_index)
{
	return _return_cross_bar.peek(port_index);
}

const MemoryReturn UnitBlockingCache::read_return(uint port_index)
{
	return _return_cross_bar.read(port_index);
}

}}
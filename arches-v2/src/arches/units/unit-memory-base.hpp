#pragma once

#include "../../stdafx.hpp"

#include "unit-base.hpp"

namespace Arches { namespace Units {

#define CACHE_LINE_SIZE 32

struct MemoryRequestItem
{
	enum Type : uint8_t
	{
		STORE,
		STORE_ACCEPT,
		LOAD,
		LOAD_RETURN,
	};

	//return loaction for loads
	uint32_t         return_buffer_id; //where should we return this to
	uint32_t         return_port;

	//meta data 
	Type             type;
	bool             sign_extend;
	uint8_t          dst_reg;
	uint8_t          dst_reg_file;
	uint8_t          size;
	uint8_t          _reserved[3];

	physical_address paddr;
	uint8_t          data[CACHE_LINE_SIZE];//make sure this is 8 byte aligned
};

class UnitMemoryBase : public UnitBase
{
protected:
	InputBuffer<MemoryRequestItem> _request_buffer;
	InputBuffer<MemoryRequestItem> _return_buffer{1};
	uint32_t                       _memory_higher_buffer_id{0};

public:
	UnitMemoryBase(UnitMemoryBase* memory_higher, uint32_t num_clients, Simulator* simulator) : UnitBase(simulator), _request_buffer(num_clients) 
	{
		simulator->register_input_buffer(&_request_buffer);
		simulator->register_input_buffer(&_return_buffer);
		if(memory_higher) _memory_higher_buffer_id = memory_higher->get_request_buffer_id();
	}

	uint i{sizeof(MemoryRequestItem)};

	uint32_t get_request_buffer_id() { return _request_buffer.id; }
};

}}



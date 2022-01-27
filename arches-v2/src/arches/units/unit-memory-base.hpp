#pragma once

#include "../../stdafx.hpp"

#include "unit-base.hpp"

namespace Arches { namespace Units {

#define CACHE_LINE_SIZE 32

struct MemoryRequestItem
{
	enum class Type : uint8_t
	{
		STORE,
		LOAD,
		LOAD_RETURN,
	};

	//return loaction for loads
	buffer_id_t      return_buffer_id_stack[4]{0, 0, 0, 0}; //where should we return this to load return are always to port zero for now
	uint8_t          return_buffer_id_stack_size{0};
	//uint8_t        return_port_id[4];

	//meta data 
	Type             type{Type::STORE};
	bool             sign_extend{false};
	uint8_t          dst_reg{0};
	uint8_t          dst_reg_file{0};
	uint8_t          size{0};
	uint8_t          offset{0};
	uint8_t          _padd[1];

	paddr_t          paddr{0ull};
	uint8_t          data[CACHE_LINE_SIZE];//make sure this is 8 byte aligned
};

#define TEMP (sizeof(MemoryRequestItem) == 64

class UnitMemoryBase : public UnitBase
{
protected:
	InputBuffer<MemoryRequestItem> _request_buffer{0};
	InputBuffer<MemoryRequestItem> _return_buffer{1};

	buffer_id_t                       _memory_higher_buffer_id{0};
	client_id_t                       _client_id{0};

public:
	UnitMemoryBase(UnitMemoryBase* memory_higher, Simulator* simulator) : UnitBase(simulator)
	{
		simulator->register_input_buffer(&_request_buffer);
		simulator->register_input_buffer(&_return_buffer);
		if(memory_higher)
		{
			_memory_higher_buffer_id = memory_higher->get_request_buffer_id();
			_client_id = memory_higher->add_client();
		}
	}

	buffer_id_t get_request_buffer_id() { return _request_buffer.id; }
	client_id_t add_client()
	{ 
		client_id_t client_id = _request_buffer.size();
		_request_buffer.resize(client_id + 1);
		return client_id;
	}
};

}}



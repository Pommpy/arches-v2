#pragma once

#include "../../stdafx.hpp"

#include "unit-base.hpp"

namespace Arches { namespace Units {

#define CACHE_LINE_SIZE 64

struct MemoryRequestItem
{
	enum class Type : uint8_t
	{
		STORE,
		LOAD,
		LOAD_RETURN,
	};

	//meta data 
	Type             type{Type::STORE};
	uint8_t          size{0};
	uint8_t          offset{0};

	bool             sign_extend{false};
	uint8_t          dst_reg{0};
	uint8_t          dst_reg_file{0};
	uint8_t          _pad[2];
	
	paddr_t          line_paddr{0ull};
	union
	{
		uint8_t  data[CACHE_LINE_SIZE];//make sure this is 8 byte aligned

		uint16_t _data_u16[CACHE_LINE_SIZE/2];
		uint32_t _data_u32[CACHE_LINE_SIZE/4];
		uint64_t _data_u64[CACHE_LINE_SIZE/8];

		float    _data_f32[CACHE_LINE_SIZE/4];
		double   _data_f64[CACHE_LINE_SIZE/8];
	};
};

//struct MemoryReturnItem
//{
//	MemoryRequestItem request;
//	uint8_t           data[CACHE_LINE_SIZE];//make sure this is 8 byte aligned
//};

class UnitMemoryBase : public UnitBase
{
public:
	BusGroup<MemoryRequestItem> request_bus;
	BusGroup<MemoryRequestItem> return_bus;
	uint                        offset_bits{ 0 }; //how many bits are used for the offset. Needed by the core to align loads to line boundries properly
	uint64_t                    offset_mask{ 0 };

	UnitMemoryBase(uint num_clients, Simulator* simulator) : request_bus(num_clients), return_bus(num_clients), UnitBase(simulator)
 	{
		
	}
};

}}



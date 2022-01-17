#pragma once

#include "../../stdafx.hpp"

#include "unit-base.hpp"

namespace Arches { namespace Units {

struct ExecutionItem 
{
	uint32_t instr;
	uint32_t rs1_value;
	uint32_t rs2_value;
	uint32_t rs3_value;
	uint8_t  rs1;
	uint8_t  rs2;
	uint8_t  rs3;
	uint8_t  rd;
};

struct WriteBackItem
{
	uint32_t rd_value;
	uint8_t  rd;
};

}}
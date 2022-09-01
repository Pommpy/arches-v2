#pragma once

#include "../../stdafx.hpp"

#include "unit-base.hpp"

namespace Arches { namespace Units {

struct ExecutionItem 
{
	uint32_t instr;
	uint8_t  rd;
};

struct WriteBackItem
{
	uint8_t dst_reg_file;
	uint8_t dst_reg;
};

}}
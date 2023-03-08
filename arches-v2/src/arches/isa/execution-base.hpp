#pragma once

#include "../../stdafx.hpp"

#include "riscv.hpp"

#include "registers.hpp"

#include "../units/unit-memory-base.hpp"

namespace Arches { namespace ISA { namespace RISCV {

	struct RegAddr
	{
		uint8_t reg : 5;
		uint8_t reg_file : 2;
		uint8_t sign_ext : 1;
	};

	class ExecutionBase
	{
	public:
		uint8_t*                   backing_memory{nullptr};
		IntegerRegisterFile*       int_regs{nullptr};
		FloatingPointRegisterFile* float_regs{nullptr};

		Arches::vaddr_t pc{0};
		bool branch_taken {false};

		struct
		{
			Units::MemoryRequest::Type type;
			RegAddr dst;
			uint8_t size;
			bool cached;

			paddr_t vaddr;
			uint8_t store_data[CACHE_LINE_SIZE];
		}mem_req;

		ExecutionBase(IntegerRegisterFile* int_regs, FloatingPointRegisterFile* float_regs) : int_regs(int_regs), float_regs(float_regs) {}

		void _write_register(RegAddr dst, uint8_t size, const uint8_t* data)
		{
			if(dst.reg_file == 0)
			{
				//TODO fix. This is not portable.
				//int_regs->registers[dst_reg].u64 = 0x0ull;
				if(!dst.sign_ext)
				{
					switch(size)
					{
					case 1: int_regs->registers[dst.reg].u64 = *((uint8_t*)data); break;
					case 2: int_regs->registers[dst.reg].u64 = *((uint16_t*)data); break;
					case 4: int_regs->registers[dst.reg].u64 = *((uint32_t*)data); break;
					case 8: int_regs->registers[dst.reg].u64 = *((uint64_t*)data); break;
						nodefault;
					}
				}
				else
				{
					switch(size)
					{
					case 1: int_regs->registers[dst.reg].s64 = *((int8_t*)data); break;
					case 2: int_regs->registers[dst.reg].s64 = *((int16_t*)data); break;
					case 4: int_regs->registers[dst.reg].s64 = *((int32_t*)data); break;
					case 8: int_regs->registers[dst.reg].s64 = *((int64_t*)data); break;
						nodefault;
					}
				}
			}
			else if(dst.reg_file == 1)
			{
				assert(size == 4);
				float_regs->registers[dst.reg].f32 = *((float*)data);
			}
		}

		ExecutionBase() {}
		virtual ~ExecutionBase() = default;
	};

}}}

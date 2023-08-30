#pragma once

#include "../../stdafx.hpp"

#include "riscv.hpp"

#include "registers.hpp"

#include "../units/unit-memory-base.hpp"

namespace Arches { namespace ISA { namespace RISCV {

	class ExecutionBase
	{
	public:
		uint8_t*                   backing_memory{nullptr};
		IntegerRegisterFile*       int_regs{nullptr};
		FloatingPointRegisterFile* float_regs{nullptr};

		Arches::vaddr_t pc{0};
		bool branch_taken {false};

		MemoryRequest mem_req;

		ExecutionBase(IntegerRegisterFile* int_regs, FloatingPointRegisterFile* float_regs) : int_regs(int_regs), float_regs(float_regs) {}

		void _write_register(RegAddr dst, const uint8_t* src, uint8_t size)
		{
			if(dst.reg_type == 0)
			{
				//TODO fix. This is not portable.
				//int_regs->registers[dst_reg].u64 = 0x0ull;
				if(!dst.sign_ext)
				{
					switch(size)
					{
					case 1: int_regs->registers[dst.reg].u64 = *((uint8_t*)src); break;
					case 2: int_regs->registers[dst.reg].u64 = *((uint16_t*)src); break;
					case 4: int_regs->registers[dst.reg].u64 = *((uint32_t*)src); break;
					case 8: int_regs->registers[dst.reg].u64 = *((uint64_t*)src); break;
						nodefault;
					}
				}
				else
				{
					switch(size)
					{
					case 1: int_regs->registers[dst.reg].s64 = *((int8_t*)src); break;
					case 2: int_regs->registers[dst.reg].s64 = *((int16_t*)src); break;
					case 4: int_regs->registers[dst.reg].s64 = *((int32_t*)src); break;
					case 8: int_regs->registers[dst.reg].s64 = *((uint64_t*)src); break;
						nodefault;
					}
				}
			}
			else if(dst.reg_type == 1)
			{
				assert(size == 4);
				float_regs->registers[dst.reg].f32 = *((float*)src);
			}
		}

		ExecutionBase() {}
		virtual ~ExecutionBase() = default;
	};

}}}

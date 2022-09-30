#pragma once

#include "../../stdafx.hpp"

#include "riscv.hpp"

#include "registers.hpp"

namespace Arches { namespace ISA { namespace RISCV {

	class ExecutionBase
	{
	public:
		void*                      backing_memory{nullptr};
		IntegerRegisterFile*       int_regs{nullptr};
		FloatingPointRegisterFile* float_regs{nullptr};

		Arches::vaddr_t pc{0};
		bool branch_taken {false};

		struct
		{
			Arches::vaddr_t vaddr{0};

			union
			{
				struct
				{
					uint8_t dst_reg;
					uint8_t dst_reg_file;
					bool    sign_extend;
				};
				union
				{
					uint64_t store_data;
					uint8_t  store_data_u8[8];
				};
			};

			uint8_t size{0};
		}
		memory_access_data;	

		ExecutionBase(IntegerRegisterFile* int_regs, FloatingPointRegisterFile* float_regs) : int_regs(int_regs), float_regs(float_regs) {}

	protected:
		void _write_register(uint dst_reg, uint dst_reg_file, uint size, bool sign_extend, const uint8_t* data)
		{
			if(dst_reg_file == 0)
			{
				//TODO fix. This is not portable.
				//int_regs->registers[dst_reg].u64 = 0x0ull;
				if(!sign_extend)
				{
					switch(size)
					{
					case 1: int_regs->registers[dst_reg].u64 = *((uint8_t*)data); break;
					case 2: int_regs->registers[dst_reg].u64 = *((uint16_t*)data); break;
					case 4: int_regs->registers[dst_reg].u64 = *((uint32_t*)data); break;
					case 8: int_regs->registers[dst_reg].u64 = *((uint64_t*)data); break;
					nodefault;
					}
				}
				else
				{
					switch(size)
					{
					case 1: int_regs->registers[dst_reg].s64 = *((int8_t*)data); break;
					case 2: int_regs->registers[dst_reg].s64 = *((int16_t*)data); break;
					case 4: int_regs->registers[dst_reg].s64 = *((int32_t*)data); break;
					case 8: int_regs->registers[dst_reg].s64 = *((int64_t*)data); break;
					nodefault;
					}
				}

				int_regs->valid[dst_reg] = true;
			}
			else if(dst_reg_file == 1)
			{
				assert(size == 4);
				float_regs->registers[dst_reg].f32 = *((float*)data);
				float_regs->valid[dst_reg] = true;
			}
		}

		bool _check_dependacies_and_set_valid_bit(const ISA::RISCV::Instruction instr, ISA::RISCV::InstructionInfo const& instr_info)
		{
			bool* dst_valid = (uint8_t)instr_info.dst_reg_file ? float_regs->valid : int_regs->valid;
			bool* src_valid = (uint8_t)instr_info.src_reg_file ? float_regs->valid : int_regs->valid;

			switch (instr_info.encoding)
			{
			case ISA::RISCV::Encoding::R:
				if (dst_valid[instr.rd] && src_valid[instr.rs1] && src_valid[instr.rs2])
				{
					dst_valid[instr.rd] = false;
					int_regs->valid[0] = true;
					return true;
				}
				return false;

			case ISA::RISCV::Encoding::R4:
				if (dst_valid[instr.rd] && src_valid[instr.rs1] && src_valid[instr.rs2] && src_valid[instr.rs3])
				{
					dst_valid[instr.rd] = false;
					int_regs->valid[0] = true;
					return true;
				}
				return false;

			case ISA::RISCV::Encoding::I:
				if (dst_valid[instr.rd] && src_valid[instr.rs1])
				{
					dst_valid[instr.rd] = false;
					int_regs->valid[0] = true;
					return true;
				}
				return false;

			case ISA::RISCV::Encoding::S:
				if (dst_valid[instr.rs2] && src_valid[instr.rs1])
				{
					return true;
				}
				return false;

			case ISA::RISCV::Encoding::B:
				if (src_valid[instr.rs1] && src_valid[instr.rs2])
				{
					return true;
				}
				return false;

			case ISA::RISCV::Encoding::U:
				if (dst_valid[instr.rd])
				{
					dst_valid[instr.rd] = false;
					int_regs->valid[0] = true;
					return true;
				}
				return false;

			case ISA::RISCV::Encoding::J:
				if (dst_valid[instr.rd])
				{
					dst_valid[instr.rd] = false;
					int_regs->valid[0] = true;
					return true;
				}
				return false;
			}

			return false;
		}

	public:
		ExecutionBase() {}
		virtual ~ExecutionBase() = default;
	};

}}}

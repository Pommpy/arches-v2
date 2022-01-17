#pragma once

#include "../../stdafx.hpp"

#include "riscv.hpp"

#include "registers.hpp"

namespace Arches { namespace ISA { namespace RISCV {

	class ExecutionBase
	{
	public:
		void*                      backing_memory{nullptr};
		IntegerRegisterFile*       int_regs;
		FloatingPointRegisterFile* float_regs;

		bool branch_taken {false};
		Arches::virtual_address pc{0};

		struct
		{
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
			Arches::virtual_address vaddr{0};
		}
		memory_access_data;	

		ExecutionBase(IntegerRegisterFile* int_regs, FloatingPointRegisterFile* float_regs) : int_regs(int_regs), float_regs(float_regs) {}

	protected:
		bool _check_dependacies_and_set_valid_bit(const ISA::RISCV::Instruction instr, ISA::RISCV::InstructionData const& instr_data)
		{
			bool* dst_valid = (uint8_t)instr_data.dst_reg_file ? float_regs->valid : int_regs->valid;
			bool* src_valid = (uint8_t)instr_data.src_reg_file ? float_regs->valid : int_regs->valid;

			switch (instr_data.encoding)
			{
			case ISA::RISCV::Encoding::R:
				if (dst_valid[instr.rd] && src_valid[instr.rs1] && src_valid[instr.rs2])
				{
					dst_valid[instr.rd] = false;
					int_regs->valid[0] = true;
					return true;
				}
				break;

			case ISA::RISCV::Encoding::R4:
				if (dst_valid[instr.rd] && src_valid[instr.rs1] && src_valid[instr.rs2] && src_valid[instr.rs3])
				{
					dst_valid[instr.rd] = false;
					int_regs->valid[0] = true;
					return true;
				}
				break;

			case ISA::RISCV::Encoding::I:
				if (dst_valid[instr.rd] && src_valid[instr.rs1])
				{
					dst_valid[instr.rd] = false;
					int_regs->valid[0] = true;
					return true;
				}
				break;

			case ISA::RISCV::Encoding::S:
				if (dst_valid[instr.rs2] && src_valid[instr.rs1])
				{
					return true;
				}
				break;

			case ISA::RISCV::Encoding::B:
				if (src_valid[instr.rs1] && src_valid[instr.rs2])
				{
					return true;
				}
				break;

			case ISA::RISCV::Encoding::U:
				if (dst_valid[instr.rd])
				{
					dst_valid[instr.rd] = false;
					int_regs->valid[0] = true;
					return true;
				}
				break;

			case ISA::RISCV::Encoding::J:
				if (dst_valid[instr.rd])
				{
					dst_valid[instr.rd] = false;
					int_regs->valid[0] = true;
					return true;
				}
				break;
			}

			return false;
		}

	public:
		ExecutionBase() {}
		virtual ~ExecutionBase() = default;
	};

}}}

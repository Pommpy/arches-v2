#pragma once

#include "../../stdafx.hpp"

#include "unit-base.hpp"
#include "unit-memory-base.hpp"
#include "unit-main-memory-base.hpp"
#include "uint-atomic-increment.hpp"

#include "../isa/execution-base.hpp"
#include "../isa/registers.hpp"

namespace Arches { namespace ISA { namespace RISCV {

//TRAXAMOIN
const static InstructionInfo traxamoin(0b00010, "traxamoin", Type::CUSTOM, Encoding::U, RegFile::INT, IMPL_DECL
{
	unit->memory_access_data.dst_reg_file = 0;
	unit->memory_access_data.dst_reg = instr.i.rd;
	unit->memory_access_data.sign_extend = false;
	unit->memory_access_data.size = 4;
});

}}}

namespace Arches { namespace Units {


class UnitCoreSimple : public ISA::RISCV::ExecutionBase, public UnitBase
{
public:
	class Log
	{
	protected:
		uint64_t _instruction_counters[static_cast<size_t>(ISA::RISCV::Type::NUM_TYPES)];

	public:
		Log() { reset(); }

		void reset()
		{
			for(uint i = 0; i < static_cast<size_t>(ISA::RISCV::Type::NUM_TYPES); ++i)
				_instruction_counters[i] = 0;
		}

		void acculuate(const Log& other)
		{
			for(uint i = 0; i < static_cast<size_t>(ISA::RISCV::Type::NUM_TYPES); ++i)
				_instruction_counters[i] += other._instruction_counters[i];
		}

		void log_instruction(const ISA::RISCV::InstructionInfo& info)
		{
			_instruction_counters[static_cast<size_t>(info.type)]++;
		}

		void print_log()
		{
			uint64_t total = 0;
			for(uint i = 0; i < static_cast<size_t>(ISA::RISCV::Type::NUM_TYPES); ++i)
			{
				printf("%s: %lld\n", ISA::RISCV::instr_type_names[i].c_str(), _instruction_counters[i]);
				total += _instruction_counters[i];
			}

			printf("Total: %lld\n", total);
		}
	}log;

private:
	ISA::RISCV::IntegerRegisterFile _int_regs{};
	ISA::RISCV::FloatingPointRegisterFile _float_regs{};

	UnitAtomicIncrement* atomic_inc;
	UnitMemoryBase* mem_higher;
	UnitMainMemoryBase* main_mem;

	uint tm_index;
	uint global_index;

	uint                        offset_bits{0}; //how many bits are used for the offset. Needed by the core to align loads to line boundries properly
	uint64_t                    offset_mask{0};

	uint8_t _stack[1024];
	paddr_t _stack_start{ 0ull - sizeof(_stack) };

	bool _stalled_for_store{false};
	bool _stalled_for_load{false};
	
public:
	//this is how you declare the size of output buffer. In this case we want to send one memory request item on any given cycle
	//also we must register our input buffers in contructor by convention
	UnitCoreSimple(UnitMemoryBase* mem_higher, UnitMainMemoryBase* main_mem, UnitAtomicIncrement* atomic_inc, uint tm_index, uint chip_index, Simulator* simulator);

	void clock_rise() override;
	void clock_fall() override;
};

}}
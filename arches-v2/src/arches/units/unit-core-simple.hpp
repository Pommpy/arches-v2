#pragma once

#include "../../stdafx.hpp"

#include "unit-base.hpp"
#include "unit-execution-base.hpp"
#include "unit-memory-base.hpp"
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


class UnitCoreSimple : public ISA::RISCV::ExecutionBase, public UnitMemoryBase
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

		void log_instruction(const ISA::RISCV::InstructionData& data)
		{
			_instruction_counters[static_cast<size_t>(data.type)]++;
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

	buffer_id_t _atomic_inc_buffer_id;
	client_id_t _atomic_inc_client_id;

	buffer_id_t _main_mem_buffer_id;
	client_id_t _main_mem_client_id;

	uint8_t _stack[1024];
	paddr_t _stack_start{ 0ull - sizeof(_stack) };

	bool _stalled_for_store{false};
	bool _stalled_for_load{false};
	
public:
	//this is how you declare the size of output buffer. In this case we want to send one memory request item on any given cycle
	//also we must register our input buffers in contructor by convention
	UnitCoreSimple(UnitMemoryBase* mem_higher, UnitMemoryBase* main_memory, UnitAtomicIncrement* atomic_inc, Simulator* simulator);

	void acknowledge(buffer_id_t buffer) override;

	void execute() override;
};

}}
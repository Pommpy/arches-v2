#pragma once

#include "../../../stdafx.hpp"

#include "../unit-base.hpp"
#include "../unit-memory-base.hpp"
#include "../unit-main-memory-base.hpp"
#include "../uint-atomic-increment.hpp"

#include "../../isa/execution-base.hpp"
#include "../../isa/registers.hpp"

#include "../unit-sfu.hpp"

namespace Arches { namespace ISA { namespace RISCV { namespace TRaX {

//TRAXAMOIN
const static InstructionInfo traxamoin(0b00010, "traxamoin", Type::CUSTOM0, Encoding::U, RegFile::INT, IMPL_DECL
{
	unit->memory_access_data.dst_reg_file = 0;
	unit->memory_access_data.dst_reg = instr.i.rd;
	unit->memory_access_data.sign_extend = false;
	unit->memory_access_data.size = 4;
});

}}}}

namespace Arches { namespace Units { namespace TRaX {

class UnitTP : public UnitBase, public ISA::RISCV::ExecutionBase
{
public:
	struct Configuration
	{
		uint tm_index;
		uint global_index;

		UnitMainMemoryBase*  main_mem;
		UnitMemoryBase*      mem_higher;
		UnitAtomicRegfile* atomic_inc;
		UnitSFU**            sfu_table;
	};

	class Log
	{
	protected:
		uint64_t _instruction_counters[static_cast<size_t>(ISA::RISCV::Type::NUM_TYPES)];
		uint64_t _stall_counters[static_cast<size_t>(ISA::RISCV::Type::NUM_TYPES)];

	public:
		Log() { reset(); }

		void reset()
		{
			for(uint i = 0; i < static_cast<size_t>(ISA::RISCV::Type::NUM_TYPES); ++i)
			{
				_instruction_counters[i] = 0;
				_stall_counters[i] = 0;
			}
		}

		void accumulate(const Log& other)
		{
			for(uint i = 0; i < static_cast<size_t>(ISA::RISCV::Type::NUM_TYPES); ++i)
			{
				_instruction_counters[i] += other._instruction_counters[i];
				_stall_counters[i] += other._stall_counters[i];
			}
		}

		void log_instruction(const ISA::RISCV::InstructionInfo& info)
		{
			_instruction_counters[static_cast<size_t>(info.type)]++;
		}

		void log_stall(const ISA::RISCV::InstructionInfo& info)
		{
			_stall_counters[static_cast<size_t>(info.type)]++;
		}

		void print_log(uint num_units = 1)
		{
			uint64_t total = 0;

			std::vector<std::pair<const char*, uint64_t>> _instruction_counter_pairs;
			for(uint i = 0; i < static_cast<size_t>(ISA::RISCV::Type::NUM_TYPES); ++i)
			{
				total += _instruction_counters[i];
				_instruction_counter_pairs.push_back({ISA::RISCV::instr_type_names[i].c_str(), _instruction_counters[i]});
			}
			std::sort(_instruction_counter_pairs.begin(), _instruction_counter_pairs.end(),
				[](const std::pair<const char*, uint64_t>& a, const std::pair<const char*, uint64_t>& b) -> bool { return a.second > b.second; });

			printf("Instruction mix\n");
			printf("\tTotal: %lld\n", total / num_units);
			for(uint i = 0; i < _instruction_counter_pairs.size(); ++i)
				if(_instruction_counter_pairs[i].second) printf("\t%s: %lld (%.2f%%)\n", _instruction_counter_pairs[i].first, _instruction_counter_pairs[i].second / num_units, static_cast<float>(_instruction_counter_pairs[i].second) / total * 100.0f);

			total = 0;

			std::vector<std::pair<const char*, uint64_t>> _stall_counter_pairs;
			for(uint i = 0; i < static_cast<size_t>(ISA::RISCV::Type::NUM_TYPES); ++i)
			{
				total += _stall_counters[i];
				_stall_counter_pairs.push_back({ISA::RISCV::instr_type_names[i].c_str(), _stall_counters[i]});
			}
			std::sort(_stall_counter_pairs.begin(), _stall_counter_pairs.end(),
				[](const std::pair<const char*, uint64_t>& a, const std::pair<const char*, uint64_t>& b) -> bool { return a.second > b.second; });

			printf("Resoruce Stalls\n");
			printf("\tTotal: %lld\n", total / num_units);
			for(uint i = 0; i < _stall_counter_pairs.size(); ++i)
				if(_stall_counter_pairs[i].second) printf("\t%s: %lld (%.2f%%)\n", _stall_counter_pairs[i].first, _stall_counter_pairs[i].second / num_units, static_cast<float>(_stall_counter_pairs[i].second) / total * 100.0f);
		}
	}log;

	paddr_t stack_end;

private:
	ISA::RISCV::IntegerRegisterFile       _int_regs{};
	ISA::RISCV::FloatingPointRegisterFile _float_regs{};

	UnitAtomicRegfile* atomic_inc;
	UnitMemoryBase*      mem_higher;
	UnitMainMemoryBase*  main_mem;
	UnitSFU**            sfu_table;

	uint tm_index;
	uint global_index;

	uint64_t offset_mask{0}; //Needed by the core to align loads to line boundries properly

	ISA::RISCV::Instruction last_instr{0};
	ISA::RISCV::InstructionInfo last_instr_info;
	UnitSFU* _last_issue_sfu{nullptr};

	bool _stalled_for_store_issue{false};
	bool _stalled_for_load_issue{false};
	bool _pending_load{false};
	bool _stalled_for_atomic_inc_issue{false};

public:
	UnitTP(const Configuration& config, Simulator* simulator);

	void clock_rise() override;
	void clock_fall() override;

private:
	void _process_load_return(const MemoryRequestItem& return_item);
	bool _check_dependacies_and_set_valid_bit(const ISA::RISCV::Instruction instr, ISA::RISCV::InstructionInfo const& instr_info);
};

}}}
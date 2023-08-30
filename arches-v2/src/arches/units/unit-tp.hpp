#pragma once

#include "../../stdafx.hpp"

#include "unit-base.hpp"
#include "unit-memory-base.hpp"
#include "unit-main-memory-base.hpp"
#include "unit-atomic-reg-file.hpp"

#include "../isa/execution-base.hpp"
#include "../isa/registers.hpp"

#include "../util/bit-manipulation.hpp"

#include "unit-sfu.hpp"

namespace Arches { namespace Units {

class UnitTP : public UnitBase, public ISA::RISCV::ExecutionBase
{
public:
	struct Configuration
	{
		vaddr_t pc{0x0};
		vaddr_t sp{0x0};

		uint8_t* backing_memory{nullptr};

		uint tp_index{0};
		uint tm_index{0};

		uint stack_size{512};

		uint port_size{16};

		MemoryMap mem_map{};

		UnitSFU**     sfu_table{nullptr};
	};

private:
	ISA::RISCV::IntegerRegisterFile       _int_regs{};
	ISA::RISCV::FloatingPointRegisterFile _float_regs{};

	uint8_t _float_regs_pending[32];
	uint8_t _int_regs_pending[32];

	std::queue<MemoryRequest> _lsq;
	bool _stalled_for_lsq;

	uint _tp_index;
	uint _tm_index;

	ISA::RISCV::Instruction _last_instr{0};
	ISA::RISCV::InstructionInfo _last_instr_info;

	MemoryMap mem_map;

	UnitSFU* _last_issue_sfu{nullptr};
	UnitSFU** sfu_table;

	std::map<UnitSFU*, uint> _issued_sfus;
	std::map<uint, uint> _issued_mus_indices;

	std::vector<uint8_t> _stack_mem;
	uint64_t _stack_mask;

	uint     _port_size;
	uint64_t _port_mask;

public:
	UnitTP(const Configuration& config);

	void clock_rise() override;
	void clock_fall() override;

private:
	void _process_load_return(const MemoryReturn& ret);
	uint8_t _check_dependancies(const ISA::RISCV::Instruction instr, ISA::RISCV::InstructionInfo const& instr_info);
	void _clear_register_pending(const ISA::RISCV::RegAddr& dst);
	paddr_t _get_load_port_address(paddr_t paddr) { return paddr & _port_mask; };
	void _try_drain_lsq();

public:
	class Log
	{
	protected:
		vaddr_t _elf_start_addr;
		std::vector<uint64_t> _profile_counters;
		uint _instr_index{0};

		uint64_t _instruction_counters[static_cast<size_t>(ISA::RISCV::Type::NUM_TYPES)];
		uint64_t _resource_stall_counters[static_cast<size_t>(ISA::RISCV::Type::NUM_TYPES)];
		uint64_t _data_stall_counters[static_cast<size_t>(ISA::RISCV::Type::NUM_TYPES)];

	public:
		Log(uint64_t elf_start_addr) : _elf_start_addr(elf_start_addr) { reset(); }

		void reset()
		{
			for(uint i = 0; i < static_cast<size_t>(ISA::RISCV::Type::NUM_TYPES); ++i)
			{
				_instruction_counters[i] = 0;
				_resource_stall_counters[i] = 0;
				_data_stall_counters[i] = 0;
				_profile_counters.clear();
			}
		}

		void accumulate(const Log& other)
		{
			for(uint i = 0; i < static_cast<size_t>(ISA::RISCV::Type::NUM_TYPES); ++i)
			{
				_instruction_counters[i] += other._instruction_counters[i];
				_resource_stall_counters[i] += other._resource_stall_counters[i];
				_data_stall_counters[i] += other._data_stall_counters[i];
			}

			_profile_counters.resize(std::max(_profile_counters.size(), other._profile_counters.size()), 0ull);
			for(uint i = 0; i < other._profile_counters.size(); ++i)
			{
				_profile_counters[i] += other._profile_counters[i];
			}
		}

		void log_cycle()
		{
			_profile_counters[_instr_index]++;
		}

		void log_instruction_issue(const ISA::RISCV::InstructionInfo& info, vaddr_t pc)
		{
			_instruction_counters[static_cast<size_t>(info.type)]++;

			if(pc >= _elf_start_addr)
			{
				_instr_index = (pc - _elf_start_addr) / 4;
				if(_instr_index >= _profile_counters.size()) _profile_counters.resize(_instr_index + 1, 0ull);
			}

			log_cycle();
		}

		void log_resource_stall(const ISA::RISCV::InstructionInfo& info)
		{
			_resource_stall_counters[static_cast<size_t>(info.type)]++;

			log_cycle();
		}

		void log_data_stall(uint8_t type)
		{
			_data_stall_counters[type]++;

			log_cycle();
		}

		void print_log(FILE* stream = stdout, uint num_units = 1)
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

			fprintf(stream, "Instruction mix\n");
			fprintf(stream, "\tTotal: %lld\n", total / num_units);
			for(uint i = 0; i < _instruction_counter_pairs.size(); ++i)
				if(_instruction_counter_pairs[i].second) fprintf(stream, "\t%s: %lld (%.2f%%)\n", _instruction_counter_pairs[i].first, _instruction_counter_pairs[i].second / num_units, static_cast<float>(_instruction_counter_pairs[i].second) / total * 100.0f);

			total = 0;

			std::vector<std::pair<const char*, uint64_t>> _resource_stall_counter_pairs;
			for(uint i = 0; i < static_cast<size_t>(ISA::RISCV::Type::NUM_TYPES); ++i)
			{
				total += _resource_stall_counters[i];
				_resource_stall_counter_pairs.push_back({ISA::RISCV::instr_type_names[i].c_str(), _resource_stall_counters[i]});
			}
			std::sort(_resource_stall_counter_pairs.begin(), _resource_stall_counter_pairs.end(),
				[](const std::pair<const char*, uint64_t>& a, const std::pair<const char*, uint64_t>& b) -> bool { return a.second > b.second; });

			fprintf(stream, "Resoruce Stalls\n");
			fprintf(stream, "\tTotal: %lld\n", total / num_units);
			for(uint i = 0; i < _resource_stall_counter_pairs.size(); ++i)
				if(_resource_stall_counter_pairs[i].second) fprintf(stream, "\t%s: %lld (%.2f%%)\n", _resource_stall_counter_pairs[i].first, _resource_stall_counter_pairs[i].second / num_units, static_cast<float>(_resource_stall_counter_pairs[i].second) / total * 100.0f);

			total = 0;

			std::vector<std::pair<const char*, uint64_t>> _data_stall_counter_pairs;
			for(uint i = 0; i < static_cast<size_t>(ISA::RISCV::Type::NUM_TYPES); ++i)
			{
				total += _data_stall_counters[i];
				_data_stall_counter_pairs.push_back({ISA::RISCV::instr_type_names[i].c_str(), _data_stall_counters[i]});
			}
			std::sort(_data_stall_counter_pairs.begin(), _data_stall_counter_pairs.end(),
				[](const std::pair<const char*, uint64_t>& a, const std::pair<const char*, uint64_t>& b) -> bool { return a.second > b.second; });

			fprintf(stream, "Data Stalls\n");
			fprintf(stream, "\tTotal: %lld\n", total / num_units);
			for(uint i = 0; i < _data_stall_counter_pairs.size(); ++i)
				if(_data_stall_counter_pairs[i].second) fprintf(stream, "\t%s: %lld (%.2f%%)\n", _data_stall_counter_pairs[i].first, _data_stall_counter_pairs[i].second / num_units, static_cast<float>(_data_stall_counter_pairs[i].second) / total * 100.0f);
		}

		void print_profile(uint8_t* backing_memory, FILE* stream = stdout)
		{
			uint64_t total = 0;

			fprintf(stream, "Profile\n");
			for(uint i = 0; i < _profile_counters.size(); ++i) total += _profile_counters[i];
			for(uint i = 0; i < _profile_counters.size(); ++i)
			{
				//fetch
				if(_profile_counters[i] > 0)
				{
					vaddr_t pc = i * 4 + _elf_start_addr;

					ISA::RISCV::Instruction instr(reinterpret_cast<uint32_t*>(backing_memory)[pc / 4]);
					const ISA::RISCV::InstructionInfo instr_info = instr.get_info();

					float precent = 100.0f * (float)_profile_counters[i] / total;
					if(precent > 1.0f) fprintf(stream, "*\t");
					else fprintf(stream, " \t");

					fprintf(stream, "%05I64x(%05.02f%%):          \t", pc, precent);
					instr_info.print_instr(instr, stream);
					fprintf(stream, "\n");
				}
			}
		}
	}log;
};

}}
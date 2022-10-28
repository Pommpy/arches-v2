#pragma once

#include "../../../stdafx.hpp"

#include "../unit-base.hpp"
#include "../unit-memory-base.hpp"
#include "../unit-main-memory-base.hpp"
#include "../uint-atomic-increment.hpp"

#include "../../isa/execution-base.hpp"
#include "../../isa/registers.hpp"

#include "../unit-sfu.hpp"

#include "../../../../benchmarks/dual-streaming/src/triangle.hpp"
#include "../../../../benchmarks/dual-streaming/src/aabb.hpp"

namespace Arches { namespace Units { namespace DualStreaming {

class UnitTP : public UnitBase, public ISA::RISCV::ExecutionBase
{
public:
	struct Configuration
	{
		uint tm_index;
		uint global_index;

		UnitMainMemoryBase*  main_mem;
		UnitMemoryBase*      mem_higher;
		UnitAtomicIncrement* atomic_inc;
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
	uint8_t isec_regs[22];

private:
	ISA::RISCV::IntegerRegisterFile       _int_regs{};
	ISA::RISCV::FloatingPointRegisterFile _float_regs{};

	UnitAtomicIncrement* atomic_inc;
	UnitMemoryBase*      mem_higher;
	UnitMainMemoryBase*  main_mem;
	UnitSFU**            sfu_table;

	//rayox
	//rayox
	//rayox
	//raymin
	//raydx
	//raydx
	//raydx
	//
	//minx
	//miny
	//minz
	//maxx
	//maxy
	//maxz

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

namespace Arches { namespace ISA { namespace RISCV { namespace DualStreaming {

const static InstructionInfo isa_custom0[4] =
{
	InstructionInfo(0x0, "traxamoin", Type::CUSTOM0, Encoding::U, RegFile::INT, IMPL_DECL
	{
		unit->memory_access_data.dst_reg_file = 0;
		unit->memory_access_data.dst_reg = instr.i.rd;
		unit->memory_access_data.sign_extend = false;
		unit->memory_access_data.size = 4;
	}),
	InstructionInfo(0x1, "boxisect", Type::CUSTOM1, Encoding::U, RegFile::FLOAT, IMPL_DECL
	{
		Register32* fr = unit->float_regs->registers;

		Ray ray;
		ray.o.x = fr[3].f32;
		ray.o.y = fr[4].f32;
		ray.o.z = fr[5].f32;
		ray.t_min = fr[6].f32;
		ray.d.x = fr[7].f32;
		ray.d.y = fr[8].f32;
		ray.d.z = fr[9].f32;
		ray.t_max = fr[10].f32;

		AABB aabb;
		aabb.min.x = fr[11].f32;
		aabb.min.y = fr[12].f32;
		aabb.min.z = fr[13].f32;
		aabb.max.x = fr[14].f32;
		aabb.max.y = fr[15].f32;
		aabb.max.z = fr[16].f32;

		unit->float_regs->registers[instr.u.rd].f32 = aabb.intersect(ray, rtm::vec3(1.0f) / ray.d);
	}),
	InstructionInfo(0x2, "triisect", Type::CUSTOM2, Encoding::U, RegFile::INT, IMPL_DECL
	{	
		Register32* fr = unit->float_regs->registers;

		Hit hit;
		hit.t =     fr[0].f32;
		hit.bc[0] = fr[1].f32;
		hit.bc[1] = fr[2].f32;

		Ray ray;
		ray.o.x =   fr[3].f32;
		ray.o.y =   fr[4].f32;
		ray.o.z =   fr[5].f32;
		ray.t_min = fr[6].f32;
		ray.d.x =   fr[7].f32;
		ray.d.y =   fr[8].f32;
		ray.d.z =   fr[9].f32;
		ray.t_max = fr[10].f32;

		Triangle tri;
		tri.vrts[0].x = fr[11].f32;
		tri.vrts[0].y = fr[12].f32;
		tri.vrts[0].z = fr[13].f32;
		tri.vrts[1].x = fr[14].f32;
		tri.vrts[1].y = fr[15].f32;
		tri.vrts[1].z = fr[16].f32;
		tri.vrts[2].x = fr[17].f32;
		tri.vrts[2].y = fr[18].f32;
		tri.vrts[2].z = fr[19].f32;

		unit->int_regs->registers[instr.u.rd].u32 = tri.intersect(ray, hit);

		fr[0].f32 = hit.t;
		fr[1].f32 = hit.bc[0];
		fr[2].f32 = hit.bc[1];
	}),
};


/*
const static InstructionInfo isa_custom1[6] =
{
	InstructionInfo(CUSTOM_OPCODE1, "siarg0", Type::CUSTOM, Encoding::R4, RegFile::FLOAT, RegFile::FLOAT, IMPL_DECL
	{
		Units::DualStreaming::UnitTP * tp = reinterpret_cast<Units::DualStreaming::UnitTP*>(unit);
		uint offset = instr.r4.funct3 * 3 + 4;
		tp->isec_regs[offset + 0] = instr.r4.rs1;
		tp->isec_regs[offset + 1] = instr.r4.rs2;
		tp->isec_regs[offset + 2] = instr.r4.rs3;
		tp->isec_regs[instr.r4.funct3] = instr.r4.rd;
	}),
	InstructionInfo(CUSTOM_OPCODE1, "siarg1", Type::CUSTOM, Encoding::R4, RegFile::FLOAT, RegFile::FLOAT, IMPL_DECL
	{
		Units::DualStreaming::UnitTP * tp = reinterpret_cast<Units::DualStreaming::UnitTP*>(unit);
		uint offset = instr.r4.funct3 * 3 + 4;
		tp->isec_regs[offset + 0] = instr.r4.rs1;
		tp->isec_regs[offset + 1] = instr.r4.rs2;
		tp->isec_regs[offset + 2] = instr.r4.rs3;
		tp->isec_regs[instr.r4.funct3] = instr.r4.rd;
	}),
	InstructionInfo(CUSTOM_OPCODE1, "siarg2", Type::CUSTOM, Encoding::R4, RegFile::FLOAT, RegFile::FLOAT, IMPL_DECL
	{
		Units::DualStreaming::UnitTP * tp = reinterpret_cast<Units::DualStreaming::UnitTP*>(unit);
		uint offset = instr.r4.funct3 * 3 + 4;
		tp->isec_regs[offset + 0] = instr.r4.rs1;
		tp->isec_regs[offset + 1] = instr.r4.rs2;
		tp->isec_regs[offset + 2] = instr.r4.rs3;
		tp->isec_regs[instr.r4.funct3] = instr.r4.rd;
	}),
	InstructionInfo(CUSTOM_OPCODE1, "siarg3", Type::CUSTOM, Encoding::R4, RegFile::INT, RegFile::FLOAT, IMPL_DECL
	{
		Units::DualStreaming::UnitTP * tp = reinterpret_cast<Units::DualStreaming::UnitTP*>(unit);
		uint offset = instr.r4.funct3 * 3 + 4;
		tp->isec_regs[offset + 0] = instr.r4.rs1;
		tp->isec_regs[offset + 1] = instr.r4.rs2;
		tp->isec_regs[offset + 2] = instr.r4.rs3;
		tp->isec_regs[instr.r4.funct3] = instr.r4.rd;
	}),
	InstructionInfo(CUSTOM_OPCODE1, "siarg4", Type::CUSTOM, Encoding::R4, RegFile::INT, RegFile::FLOAT, IMPL_DECL
	{
		Units::DualStreaming::UnitTP * tp = reinterpret_cast<Units::DualStreaming::UnitTP*>(unit);
		uint offset = instr.r4.funct3 * 3 + 4;
		tp->isec_regs[offset + 0] = instr.r4.rs1;
		tp->isec_regs[offset + 1] = instr.r4.rs2;
		tp->isec_regs[offset + 2] = instr.r4.rs3;
	}),
	InstructionInfo(CUSTOM_OPCODE1, "siarg5", Type::CUSTOM, Encoding::R4, RegFile::INT, RegFile::FLOAT, IMPL_DECL
	{
		Units::DualStreaming::UnitTP * tp = reinterpret_cast<Units::DualStreaming::UnitTP*>(unit);
		uint offset = instr.r4.funct3 * 3 + 4;
		tp->isec_regs[offset + 0] = instr.r4.rs1;
		tp->isec_regs[offset + 1] = instr.r4.rs2;
		tp->isec_regs[offset + 2] = instr.r4.rs3;
	}),
};
*/

const static InstructionInfo custom0(CUSTOM_OPCODE0, META_DECL{return isa_custom0[instr.u.imm]; });
//const static InstructionInfo custom1(CUSTOM_OPCODE0, META_DECL{return isa_custom1[instr.r4.funct3];});

}}}}
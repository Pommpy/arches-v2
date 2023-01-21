#include "unit-trax-tp.hpp"

namespace Arches { namespace Units { namespace TRaX {

UnitTP::UnitTP(const Configuration& config, Simulator* simulator) :  UnitBase(simulator), ISA::RISCV::ExecutionBase(&_int_regs, &_float_regs)
{
	atomic_unit = config.atomic_unit;
	main_mem = config.main_mem;
	mem_higher = config.mem_higher;
	sfu_table = config.sfu_table;

	offset_mask = mem_higher->offset_mask;

	global_index = config.global_index;
	tm_index = config.tm_index;

	executing = true;
}

void UnitTP::_process_load_return(const MemoryRequest& return_item)
{
	assert(return_item.type == MemoryRequest::Type::LOAD_RETURN);
	assert(return_item.size <= 8);
	_write_register(return_item.dst_reg, return_item.dst_reg_file, return_item.size, return_item.sign_extend, (uint8_t*)&return_item.data_u32);
	last_traxamoin = return_item.data_u32;
}

bool UnitTP::_check_dependacies_and_set_valid_bit(const ISA::RISCV::Instruction instr, ISA::RISCV::InstructionInfo const& instr_info)
{
	bool* dst_valid = (uint8_t)instr_info.dst_reg_file ? float_regs->valid : int_regs->valid;
	bool* src_valid = (uint8_t)instr_info.src_reg_file ? float_regs->valid : int_regs->valid;

	switch(instr_info.encoding)
	{
	case ISA::RISCV::Encoding::R:
		if(dst_valid[instr.rd] && src_valid[instr.rs1] && src_valid[instr.rs2])
		{
			dst_valid[instr.rd] = false;
			int_regs->valid[0] = true;
			return true;
		}
		return false;

	case ISA::RISCV::Encoding::R4:
		if(dst_valid[instr.rd] && src_valid[instr.rs1] && src_valid[instr.rs2] && src_valid[instr.rs3])
		{
			dst_valid[instr.rd] = false;
			int_regs->valid[0] = true;
			return true;
		}
		return false;

	case ISA::RISCV::Encoding::I:
		if(dst_valid[instr.rd] && src_valid[instr.rs1])
		{
			dst_valid[instr.rd] = false;
			int_regs->valid[0] = true;
			return true;
		}
		return false;

	case ISA::RISCV::Encoding::S:
		if(dst_valid[instr.rs2] && src_valid[instr.rs1])
		{
			return true;
		}
		return false;

	case ISA::RISCV::Encoding::B:
		if(src_valid[instr.rs1] && src_valid[instr.rs2])
		{
			return true;
		}
		return false;

	case ISA::RISCV::Encoding::U:
		if(dst_valid[instr.rd])
		{
			dst_valid[instr.rd] = false;
			int_regs->valid[0] = true;
			return true;
		}
		return false;

	case ISA::RISCV::Encoding::J:
		if(dst_valid[instr.rd])
		{
			dst_valid[instr.rd] = false;
			int_regs->valid[0] = true;
			return true;
		}
		return false;
	}

	return false;
}


void UnitTP::clock_rise()
{
	if(mem_higher->return_bus.get_pending(tm_index))
	{
		//_process_load_return(mem_higher->return_bus.get_bus_data(tm_index));
		mem_higher->return_bus.clear_pending(tm_index);
	}

	if(atomic_unit->return_bus.get_pending(global_index))
	{ 
		_process_load_return(atomic_unit->return_bus.get_data(global_index));
		atomic_unit->return_bus.clear_pending(global_index);
	}

	//TODO check SFU returns
	for(uint i = 0; i < static_cast<size_t>(ISA::RISCV::Type::NUM_TYPES); ++i)
	{
		if(sfu_table[i] && sfu_table[i]->return_bus.get_pending(tm_index))
		{
			UnitSFU::Request return_item = sfu_table[i]->return_bus.get_data(tm_index);
			sfu_table[i]->return_bus.clear_pending(tm_index);

			if     (return_item.dst_reg_file == 0) int_regs->valid[return_item.dst_reg] = true;
			else if(return_item.dst_reg_file == 1) float_regs->valid[return_item.dst_reg] = true;
		}
	}
}

void UnitTP::clock_fall()
{
	if(!executing) return;

	//if the loads and stores were admitted then we don't need to stall
	if(_last_issue_sfu)
	{

		if(_last_issue_sfu->request_bus.get_pending(tm_index))
		{
			log.log_stall(last_instr_info);
			return;
		}
		else if(_last_issue_sfu->latency == 1) //simulate forwarding on single cycle instructions
		{
			if(     static_cast<uint8_t>(last_instr_info.dst_reg_file) == 0) int_regs->valid[last_instr.rd] = true;
			else if(static_cast<uint8_t>(last_instr_info.dst_reg_file) == 1) float_regs->valid[last_instr.rd] = true;
		}
	}
	_last_issue_sfu = nullptr;

	//if the instruction wasn't accepted by the sfu we must stall
	if(_stalled_for_load_issue && !mem_higher->request_bus.get_pending(tm_index)) _stalled_for_load_issue = false;
	if(_stalled_for_store_issue && !main_mem->request_bus.get_pending(global_index)) _stalled_for_store_issue = false;
	if(_stalled_for_atomic_inc_issue && !atomic_unit->request_bus.get_pending(global_index)) _stalled_for_atomic_inc_issue = false;
	if(_stalled_for_store_issue || _stalled_for_load_issue || _stalled_for_atomic_inc_issue)
	{
		log.log_stall(last_instr_info);
		return;
	}

	ISA::RISCV::Instruction instr(reinterpret_cast<uint32_t*>(backing_memory)[pc / 4]);
	const ISA::RISCV::InstructionInfo instr_info = instr.get_info();

	//if we don't have the dependecy we have to stall. TODO: we need to handel forwarding from 1 cycle sfus see UnitSFU for why.
	if(!_check_dependacies_and_set_valid_bit(instr, instr_info)) return; //todo log data dependence stalls

#if 0
	if(global_index == 0 && last_traxamoin >= 469)
	{
		printf("Executing %07I64x: %08x", pc, instr.data);
		printf(" \t(%s)", instr_info.mnemonic);
		printf("\n");
		//for (uint i = 0; i < 32; ++i) printf("Register%d: %llx\n", i, _int_regs.registers[i].u64);
		//for (uint i = 0; i < 32; ++i) printf("Float Register%d: %f\n", i, _float_regs.registers[i].f32);
	}
#endif

	log.log_instruction(instr_info);
	instr_info.execute(this, instr);
	_int_regs.zero.u64 = 0ull;

	last_instr = instr;
	last_instr_info = instr_info;

	if(!branch_taken) pc += 4;
	else branch_taken = false;


	if(instr_info.type == ISA::RISCV::Type::LOAD)
	{
		if(memory_request.paddr < stack_start)
		{
			_stalled_for_load_issue = true;
			mem_higher->request_bus.set_data(memory_request, tm_index);
			mem_higher->request_bus.set_pending(tm_index);
		}
		else
		{
			if(memory_request.dst_reg_file == 0)   int_regs->valid[memory_request.dst_reg] = true;
			else if(memory_request.dst_reg_file == 1) float_regs->valid[memory_request.dst_reg] = true;
		}
	}
	else if(instr_info.type == ISA::RISCV::Type::STORE)
	{
		if(memory_request.paddr < stack_start)
		{
			mem_higher->request_bus.set_data(memory_request, tm_index);
			mem_higher->request_bus.set_pending(tm_index);
			_stalled_for_load_issue = true;
		}
	}
	else if(instr.opcode == ISA::RISCV::CUSTOM_OPCODE0 && instr.u.imm == 0)
	{
		atomic_unit->request_bus.set_data(memory_request, global_index);
		atomic_unit->request_bus.set_pending(global_index);
		_stalled_for_atomic_inc_issue = true;
	}
	else
	{
		//issue to SFU
		UnitSFU::Request request;
		request.dst_reg = instr.rd;
		request.dst_reg_file = static_cast<uint>(instr_info.dst_reg_file);
		_last_issue_sfu = sfu_table[static_cast<uint>(instr_info.type)];
		if(_last_issue_sfu)
		{
			_last_issue_sfu->request_bus.set_data(request, tm_index);
			_last_issue_sfu->request_bus.set_pending(tm_index);
		}
		else
		{
			//because of forwarding instruction with latency 1 don't cause stalls so we don't need to simulate
			if(request.dst_reg_file == 0)      int_regs->valid[request.dst_reg] = true;
			else if(request.dst_reg_file == 1) float_regs->valid[request.dst_reg] = true;
		}
	}

	if(pc == 0)
	{
		executing = false;
	}
}

}}}
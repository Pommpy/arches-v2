#include "unit-ds-tp.hpp"

namespace Arches { namespace Units { namespace DualStreaming {

UnitTP::UnitTP(const Configuration& config, Simulator* simulator) :  UnitBase(simulator), ISA::RISCV::ExecutionBase(&_int_regs, &_float_regs)
{
	ISA::RISCV::isa[ISA::RISCV::CUSTOM_OPCODE0] = ISA::RISCV::DualStreaming::custom0;

	//ISA::RISCV::isa[ISA::RISCV::CUSTOM_OPCODE1] = ISA::RISCV::DualStreaming::custom1;

	atomic_inc = config.atomic_inc;
	main_mem = config.main_mem;
	mem_higher = config.mem_higher;
	sfu_table = config.sfu_table;

	offset_mask = mem_higher->offset_mask;

	global_index = config.global_index;
	tm_index = config.tm_index;

	executing = true;
}

void UnitTP::_process_load_return(const MemoryRequestItem& return_item)
{
	assert(return_item.type == MemoryRequestItem::Type::LOAD_RETURN);
	assert(return_item.size <= 8);
	_write_register(return_item.dst_reg, return_item.dst_reg_file, return_item.size, return_item.sign_extend, (uint8_t*)&return_item.line_paddr);
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
			if(instr.opcode == ISA::RISCV::CUSTOM_OPCODE0)
			{
				if(instr.u.imm == 1) //boxisect
				{
					for(uint i = 3; i < 17; ++i)
						if(!_float_regs.valid[i]) 
							return false;
				}
				else if(instr.u.imm == 2) //triisect
				{
					for(uint i = 0; i < 20; ++i)
						if(!_float_regs.valid[i])
							return false;

					//_float_regs.valid[0] = false; //TODO maybe?
				}
			}

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

	if(atomic_inc->return_bus.get_pending(global_index))
	{ 
		_process_load_return(atomic_inc->return_bus.get_data(global_index));
		atomic_inc->return_bus.clear_pending(global_index);
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
	if(_stalled_for_atomic_inc_issue && !atomic_inc->request_bus.get_pending(global_index)) _stalled_for_atomic_inc_issue = false;
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
	if(global_index == 0)
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
		paddr_t paddr = memory_access_data.vaddr;
		if(paddr >= stack_end)
		{
			if     (memory_access_data.dst_reg_file == 0)   int_regs->valid[memory_access_data.dst_reg] = true;
			else if(memory_access_data.dst_reg_file == 1) float_regs->valid[memory_access_data.dst_reg] = true;
		}
		else
		{
			MemoryRequestItem request_item;
			request_item.type = MemoryRequestItem::Type::LOAD;
			request_item.size = memory_access_data.size;
			request_item.offset = static_cast<uint8_t>(paddr & offset_mask);
			request_item.line_paddr = paddr - request_item.offset;
			request_item.dst_reg = memory_access_data.dst_reg;
			request_item.dst_reg_file = memory_access_data.dst_reg_file;
			request_item.sign_extend = memory_access_data.sign_extend;

			if(request_item.line_paddr > 1 * 1024 * 1024 * 1024) __debugbreak();

			mem_higher->request_bus.set_data(request_item, tm_index);
			mem_higher->request_bus.set_pending(tm_index);
			_stalled_for_load_issue = true;
		}
	}
	else if(instr_info.type == ISA::RISCV::Type::STORE)
	{
		paddr_t paddr = memory_access_data.vaddr;
		if(paddr >= stack_end)
		{
		}
		else
		{
			MemoryRequestItem request_item;
			request_item.type = MemoryRequestItem::Type::STORE;
			request_item.size = memory_access_data.size;
			request_item.offset = static_cast<uint8_t>(memory_access_data.vaddr & offset_mask);
			request_item.line_paddr = memory_access_data.vaddr - request_item.offset;
			//std::memcpy(&request_item.data[request_item.offset], memory_access_data.store_data_u8, request_item.size);

		#if 0
			main_mem->request_bus.set_bus_data(request_item, global_index);
			main_mem->request_bus.set_pending(global_index);
			_stalled_for_store_issue = true;
		#else
			mem_higher->request_bus.set_data(request_item, tm_index);
			mem_higher->request_bus.set_pending(tm_index);
			_stalled_for_load_issue = true;
		#endif
		}
	}
	else if(instr.opcode == ISA::RISCV::CUSTOM_OPCODE0 && instr.u.imm == 0)
	{
		MemoryRequestItem request_item;
		request_item.type = MemoryRequestItem::Type::LOAD;
		request_item.size = memory_access_data.size;
		request_item.offset = 0;
		request_item.dst_reg = memory_access_data.dst_reg;
		request_item.dst_reg_file = memory_access_data.dst_reg_file;
		request_item.sign_extend = memory_access_data.sign_extend;

		atomic_inc->request_bus.set_data(request_item, global_index);
		atomic_inc->request_bus.set_pending(global_index);
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
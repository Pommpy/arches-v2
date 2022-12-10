#include "unit-core-simple.hpp"

namespace Arches { namespace Units {

UnitCoreSimple::UnitCoreSimple(UnitMemoryBase* mem_higher, UnitMainMemoryBase* main_mem, UnitAtomicRegfile* atomic_inc, uint tm_index, uint global_index, Simulator* simulator) :
	ExecutionBase(&_int_regs, &_float_regs), UnitBase(simulator),
	mem_higher(mem_higher), main_mem(main_mem), atomic_inc(atomic_inc),
	tm_index(tm_index), global_index(global_index)
{
	ISA::RISCV::isa[ISA::RISCV::CUSTOM_OPCODE0] = ISA::RISCV::traxamoin;

	_int_regs.ra.u64 = 0ull;

	offset_bits = mem_higher->offset_bits;
	offset_mask = mem_higher->offset_mask;

	executing = true;
}

void UnitCoreSimple::clock_rise()
{
	if(mem_higher->return_bus.get_pending(tm_index))
	{
		const MemoryRequestItem& return_item = mem_higher->return_bus.get_data(tm_index);
		mem_higher->return_bus.clear_pending(tm_index);

		assert(return_item.type == MemoryRequestItem::Type::LOAD_RETURN);
		assert(return_item.size <= 8);
		if(memory_access_data.dst_reg_file == 0)
		{
			//TODO fix. This is not portable.
			int_regs->registers[memory_access_data.dst_reg].u64 = 0x0ull;
			//std::memcpy(&int_regs->registers[memory_access_data.dst_reg].u64, &return_item.data[return_item.offset], return_item.size);

			uint64_t sign_bit = int_regs->registers[memory_access_data.dst_reg].u64 >> (return_item.size * 8 - 1);
			if(memory_access_data.sign_extend && sign_bit)
				int_regs->registers[memory_access_data.dst_reg].u64 |= (~0x0ull << (return_item.size * 8 - 1));
		}
		else
		{
			assert(return_item.size == 4);
			//std::memcpy(&float_regs->registers[memory_access_data.dst_reg].f32, &return_item.data[return_item.offset], return_item.size);
		}

		_stalled_for_load = false;
	}

	if(atomic_inc->return_bus.get_pending(global_index))
	{
		const MemoryRequestItem& return_item = atomic_inc->return_bus.get_data(global_index);
		atomic_inc->return_bus.clear_pending(global_index);

		assert(return_item.type == MemoryRequestItem::Type::LOAD_RETURN);
		assert(return_item.size <= 8);
		if(memory_access_data.dst_reg_file == 0)
		{
			//TODO fix. This is not portable.
			int_regs->registers[memory_access_data.dst_reg].u64 = 0x0ull;
			//std::memcpy(&int_regs->registers[memory_access_data.dst_reg].u64, &return_item.data[return_item.offset], return_item.size);

			uint64_t sign_bit = int_regs->registers[memory_access_data.dst_reg].u64 >> (return_item.size * 8 - 1);
			if(memory_access_data.sign_extend && sign_bit)
				int_regs->registers[memory_access_data.dst_reg].u64 |= (~0x0ull << (return_item.size * 8 - 1));
		}
		else
		{
			assert(return_item.size == 4);
			//std::memcpy(&float_regs->registers[memory_access_data.dst_reg].f32, &return_item.data[return_item.offset], return_item.size);
		}

		_stalled_for_load = false;
	}
}

void UnitCoreSimple::clock_fall()
{
	if(_stalled_for_store && !main_mem->request_bus.get_pending(global_index)) _stalled_for_store = false;

	if(!executing) return;
	if(_stalled_for_store || _stalled_for_load) return;

	ISA::RISCV::Instruction instr(reinterpret_cast<uint32_t*>(backing_memory)[pc / 4]);
	const ISA::RISCV::InstructionInfo instr_info = instr.get_info();

#if 1
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

	if(!branch_taken) pc += 4;
	else branch_taken = false;

	if(instr.opcode == ISA::RISCV::CUSTOM_OPCODE0)
	{
		MemoryRequestItem request_item;
		request_item.type = MemoryRequestItem::Type::LOAD;
		request_item.size = memory_access_data.size;
		request_item.offset = 0;
		
		atomic_inc->request_bus.set_data(request_item, global_index);
		atomic_inc->request_bus.set_pending(global_index);
		_stalled_for_load = true;
	}

	if(instr_info.type == ISA::RISCV::Type::LOAD)
	{
		paddr_t paddr = memory_access_data.vaddr;
		if(paddr >= _stack_start)
		{
			paddr_t index = paddr - _stack_start;
			if(memory_access_data.dst_reg_file == 0)
			{
				//TODO fix. This is not portable.
				int_regs->registers[memory_access_data.dst_reg].u64 = 0x0ull;
				std::memcpy(&int_regs->registers[memory_access_data.dst_reg].u64, &_stack[index], memory_access_data.size);

				uint64_t sign_bit = int_regs->registers[memory_access_data.dst_reg].u64 >> (memory_access_data.size * 8 - 1);
				if(memory_access_data.sign_extend && sign_bit)
					int_regs->registers[memory_access_data.dst_reg].u64 |= (~0x0ull << (memory_access_data.size * 8 - 1));
			}
			else
			{
				std::memcpy(&float_regs->registers[memory_access_data.dst_reg].f32, &_stack[index], memory_access_data.size);
			}
		}
		else
		{
			MemoryRequestItem request_item;
			request_item.type = MemoryRequestItem::Type::LOAD;
			request_item.size = memory_access_data.size;
			request_item.offset = static_cast<uint8_t>(paddr & offset_mask);
			request_item.line_paddr = paddr - request_item.offset;

			mem_higher->request_bus.set_data(request_item, tm_index);
			mem_higher->request_bus.set_pending(tm_index);
			_stalled_for_load = true;
		}
	}

	if(instr_info.type == ISA::RISCV::Type::STORE)
	{
		paddr_t paddr = memory_access_data.vaddr;
		if(paddr >= _stack_start)
		{
			paddr_t index = paddr - _stack_start;
			std::memcpy(&_stack[index], memory_access_data.store_data_u8, memory_access_data.size);
		}
		else
		{
			MemoryRequestItem request_item;
			request_item.type = MemoryRequestItem::Type::STORE;
			request_item.size = memory_access_data.size;
			request_item.offset = static_cast<uint8_t>(memory_access_data.vaddr & offset_mask);
			request_item.line_paddr = memory_access_data.vaddr - request_item.offset;
			//std::memcpy(&request_item.data[request_item.offset], memory_access_data.store_data_u8, request_item.size);

			//TODO this
			main_mem->request_bus.set_data(request_item, global_index);
			main_mem->request_bus.set_pending(global_index);
			_stalled_for_store = true;
		}
	}

	if(pc == 0)
	{
		executing = false;
	}
}

}}
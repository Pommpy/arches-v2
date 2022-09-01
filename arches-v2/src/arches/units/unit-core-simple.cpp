#include "unit-core-simple.hpp"

namespace Arches { namespace Units {

UnitCoreSimple::UnitCoreSimple(UnitMemoryBase* mem_higher, UnitMemoryBase* main_memory, UnitAtomicIncrement* atomic_inc, Simulator* simulator) :
	ExecutionBase(&_int_regs, &_float_regs),
	UnitMemoryBase(mem_higher, simulator)
{
	ISA::RISCV::isa[ISA::RISCV::CUSTOM_OPCODE0] = ISA::RISCV::traxamoin;

	_atomic_inc_buffer_id = atomic_inc->get_request_buffer_id();
	_atomic_inc_client_id = atomic_inc->add_client();

	_main_mem_buffer_id = main_memory->get_request_buffer_id();
	_main_mem_client_id = main_memory->add_client();

	output_buffer.resize(1, sizeof(UnitMemoryBase));
	_int_regs.ra.u64 = 0ull;

	offset_bits = mem_higher->offset_bits;
	offset_mask = mem_higher->offset_mask;

	executing = true;
}

void UnitCoreSimple::acknowledge(buffer_id_t buffer)
{
	if(_stalled_for_store)
	{
		_stalled_for_store = false;
		execute();
	}
}

void UnitCoreSimple::execute()
{
	if(!executing) return;
	if(_stalled_for_store) return;

	uint return_index;
	_return_buffer.rest_arbitrator_round_robin();
	if((return_index = _return_buffer.get_next_index()) != ~0u)
	{
		MemoryRequestItem* return_item = _return_buffer.get_message(return_index);
		assert(return_item->type == MemoryRequestItem::Type::LOAD_RETURN);
		assert(return_item->size <= 8);
		if(memory_access_data.dst_reg_file == 0)
		{
			//TODO fix. This is not portable.
			int_regs->registers[memory_access_data.dst_reg].u64 = 0x0ull;
			std::memcpy(&int_regs->registers[memory_access_data.dst_reg].u64, &return_item->data[return_item->offset], return_item->size);

			uint64_t sign_bit = int_regs->registers[memory_access_data.dst_reg].u64 >> (return_item->size * 8 - 1);
			if(memory_access_data.sign_extend && sign_bit)
				int_regs->registers[memory_access_data.dst_reg].u64 |= (~0x0ull << (return_item->size * 8 - 1));
		}
		else
		{
			assert(return_item->size == 4);
			std::memcpy(&float_regs->registers[memory_access_data.dst_reg].f32, &return_item->data[return_item->offset], return_item->size);
		}
		_stalled_for_load = false;
		_return_buffer.clear(return_index);
	}
	if(_stalled_for_load) return;

	ISA::RISCV::Instruction instr(reinterpret_cast<uint32_t*>(backing_memory)[pc / 4]);
	const ISA::RISCV::InstructionInfo instr_info = instr.get_info();
	const ISA::RISCV::InstructionData instr_data = instr_info.get_data();

	log.log_instruction(instr_data);

#if 0
	printf("Executing %07I64x: %08x", pc, instr.data);
	printf(" \t(%s)", instr_data.mnemonic);
	printf("\n");
	//for (uint i = 0; i < 32; ++i) printf("Register%d: %llx\n", i, _int_regs.registers[i].u64);
	//for (uint i = 0; i < 32; ++i) printf("Float Register%d: %f\n", i, _float_regs.registers[i].f32);
#endif

	instr_info.execute(this, instr);
	_int_regs.zero.u64 = 0ull;

	if(!branch_taken) pc += 4;
	else branch_taken = false;

	if(instr.opcode == ISA::RISCV::CUSTOM_OPCODE0)
	{
		MemoryRequestItem request_item;
		request_item.return_buffer_id_stack[request_item.return_buffer_id_stack_size++] = _return_buffer.id;

		request_item.type = MemoryRequestItem::Type::LOAD;
		request_item.size = memory_access_data.size;
		request_item.offset = 0;

		output_buffer.push_message(&request_item, _atomic_inc_buffer_id, _atomic_inc_client_id);
		_stalled_for_load = true;
	}

	if(instr_data.type == ISA::RISCV::Type::LOAD)
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
			request_item.return_buffer_id_stack[request_item.return_buffer_id_stack_size++] = _return_buffer.id;

			request_item.type = MemoryRequestItem::Type::LOAD;
			request_item.size = memory_access_data.size;
			request_item.offset = static_cast<uint8_t>(paddr & offset_mask);
			request_item.paddr = paddr - request_item.offset;

			output_buffer.push_message(&request_item, _memory_higher_buffer_id, _client_id);
			_stalled_for_load = true;
		}
	}

	if(instr_data.type == ISA::RISCV::Type::STORE)
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
			request_item.return_buffer_id_stack[request_item.return_buffer_id_stack_size++] = _return_buffer.id;

			request_item.type = MemoryRequestItem::Type::STORE;
			request_item.size = memory_access_data.size;
			request_item.offset = static_cast<uint8_t>(memory_access_data.vaddr & offset_mask);
			request_item.paddr = memory_access_data.vaddr - request_item.offset;
			std::memcpy(&request_item.data[request_item.offset], memory_access_data.store_data_u8, request_item.size);

			output_buffer.push_message(&request_item, _main_mem_buffer_id, _main_mem_client_id);
			_stalled_for_store = true;
		}
	}

	if(pc == 0)
	{
		executing = false;
	}
}

}}
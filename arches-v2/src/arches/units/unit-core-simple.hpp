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
	unit->memory_access_data.dst_reg = instr.i.rd;
});

}}}

namespace Arches { namespace Units {


class UnitCoreSimple : public ISA::RISCV::ExecutionBase, public UnitMemoryBase
{
private:
	ISA::RISCV::IntegerRegisterFile _int_regs{};
	ISA::RISCV::FloatingPointRegisterFile _float_regs{};

	buffer_id_t _atomic_inc_buffer_id;
	client_id_t _atomic_inc_client_id;

	buffer_id_t _main_mem_buffer_id;
	client_id_t _main_mem_client_id;

	bool _stalled_for_store{false};
	bool _stalled_for_load{false};
	
public:
	//this is how you declare the size of output buffer. In this case we want to send one memory request item on any given cycle
	//also we must register our input buffers in contructor by convention
	UnitCoreSimple(UnitMemoryBase* mem_higher, UnitMemoryBase* main_memory, UnitAtomicIncrement* atomic_inc, Simulator* simulator) :
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

		executing = true;
	}

	void acknowledge(buffer_id_t buffer) override
	{
		if(_stalled_for_store)
		{
			_stalled_for_store = false;
			execute();
		}
	}

	void execute() override
	{
		if(!executing) return;
		if(_stalled_for_store) return;

		_return_buffer.rest_arbitrator_round_robin();

		uint return_index;
		if((return_index = _return_buffer.get_next_index()) != ~0u)
		{
			MemoryRequestItem* return_item = _return_buffer.get_message(return_index);
			if(return_item->type == MemoryRequestItem::Type::LOAD_RETURN)
			{
				if(return_item->dst_reg_file == 0)
				{
					//TODO fix. This is not portable.
					int_regs->registers[return_item->dst_reg].u64 = (return_item->sign_extend && (return_item->data[return_item->offset + return_item->size - 1] >> 7)) ? ~0ull : 0ull;
					std::memcpy(&int_regs->registers[return_item->dst_reg].u64, return_item->data + return_item->offset, return_item->size);
				}
				else
				{ 
					std::memcpy(&float_regs->registers[return_item->dst_reg].f32, return_item->data + return_item->offset, return_item->size);
				}
			}
			_return_buffer.clear(return_index);
			_stalled_for_load = false;
		}
		if(_stalled_for_load) return;

		ISA::RISCV::Instruction instr(reinterpret_cast<uint32_t*>(backing_memory)[pc / 4]);
		const ISA::RISCV::InstructionInfo instr_info = instr.get_info();
		const ISA::RISCV::InstructionData instr_data = instr_info.get_data();

	#if 1
		printf("Executing %07I64x: %08x", pc, instr.data);
		printf(" \t(%s)", instr_data.mnemonic);
		printf("\n");
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
			request_item.dst_reg = memory_access_data.dst_reg;
			request_item.dst_reg_file = 0;
			request_item.size = 4;
			request_item.sign_extend = false;

			output_buffer.push_message(&request_item, _atomic_inc_buffer_id , _atomic_inc_client_id);
			_stalled_for_load = true;
		}

		if(instr_data.type == ISA::RISCV::Type::LOAD)
		{
			MemoryRequestItem request_item;
			request_item.return_buffer_id_stack[request_item.return_buffer_id_stack_size++] = _return_buffer.id;

			request_item.type = MemoryRequestItem::Type::LOAD;
			request_item.dst_reg = memory_access_data.dst_reg;
			request_item.dst_reg_file = memory_access_data.dst_reg_file;
			request_item.size = memory_access_data.size;
			request_item.sign_extend = memory_access_data.sign_extend;
			request_item.paddr = memory_access_data.vaddr; //no mmu for now

			output_buffer.push_message(&request_item, _memory_higher_buffer_id, _client_id);
			_stalled_for_load = true;
		}

		if(instr_data.type == ISA::RISCV::Type::STORE)
		{
			MemoryRequestItem request_item;
			request_item.return_buffer_id_stack[request_item.return_buffer_id_stack_size++] = _return_buffer.id;

			request_item.type = MemoryRequestItem::Type::STORE;
			request_item.size = memory_access_data.size;
			request_item.paddr = memory_access_data.vaddr; //no mmu for now
			std::memcpy(&request_item.data, memory_access_data.store_data_u8, std::abs(request_item.size));

			output_buffer.push_message(&request_item, _main_mem_buffer_id, _main_mem_client_id);
			_stalled_for_store = true;
		}

		if(pc == 0)
		{
			executing = false;
		}
	}
};

}}
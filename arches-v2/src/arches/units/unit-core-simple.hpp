#pragma once

#include "../../stdafx.hpp"

#include "unit-base.hpp"
#include "unit-execution-base.hpp"
#include "unit-memory-base.hpp"

#include "../isa/execution-base.hpp"
#include "../isa/registers.hpp"

namespace Arches { namespace ISA { namespace RISCV {

//TRAXAMOIN
const static InstructionInfo traxamoin(0b00010, "traxamoin", Type::CUSTOM, Encoding::U, RegFile::INT, IMPL_DECL
{
	static std::atomic_uint traxamoin_value = 0;
	std::cout << "amoin: " << traxamoin_value << "\n";
	unit->int_regs->registers[instr.u.rd].u64 = traxamoin_value++;
	//printf("%d\n", unit->int_regs->registers[instr.u.rd].u32);
});

}}}

namespace Arches { namespace Units {


class UnitCoreSimple : public ISA::RISCV::ExecutionBase, public UnitMemoryBase
{
private:
	ISA::RISCV::IntegerRegisterFile _int_regs{};
	ISA::RISCV::FloatingPointRegisterFile _float_regs{};

	bool _stalled{false};
	
public:
	uint core_id{0};

	//this is how you declare the size of output buffer. In this case we want to send one memory request item on any given cycle
	//also we must register our input buffers in contructor by convention
	UnitCoreSimple(UnitMemoryBase* mem_higher, Simulator* simulator) : 
		ExecutionBase(&_int_regs, &_float_regs),
		UnitMemoryBase(mem_higher, 0u, simulator)
	{
		ISA::RISCV::isa[ISA::RISCV::CUSTOM_OPCODE0] = ISA::RISCV::traxamoin;

		output_buffer.resize(1, sizeof(UnitMemoryBase));
		_int_regs.ra.u64 = 0ull;

		executing = true;
	}

	void execute() override
	{
		if(!executing) return;

		MemoryRequestItem* request_return;
		_return_buffer.rest_arbitrator_round_robin();
		if(request_return = _return_buffer.get_next())
		{
			if(request_return->type == MemoryRequestItem::LOAD_RETURN)
			{
				//todo not portable
				if(request_return->dst_reg_file == 0)
				{
					int_regs->registers[request_return->dst_reg].u64 = (request_return->sign_extend && (request_return->data[request_return->size - 1] >> 7)) ? ~0ull : 0ull;
					std::memcpy(&int_regs->registers[request_return->dst_reg].u64, request_return->data, request_return->size);
				}
				else
				{ 
					std::memcpy(&float_regs->registers[request_return->dst_reg].f32, request_return->data, request_return->size);
				}
			}
			_return_buffer.clear_current();
			_stalled = false;
		}

		if(_stalled) return;

		ISA::RISCV::Instruction instr(reinterpret_cast<uint32_t*>(backing_memory)[pc / 4]);
		const ISA::RISCV::InstructionInfo instr_info = instr.get_info();
		const ISA::RISCV::InstructionData instr_data = instr_info.get_data();

	#if 0
		printf("Executing %07I64x: %08x", pc, instr.data);
		printf(" \t(%s)", instr_data.mnemonic);
		printf("\n");
	#endif

		if(instr.opcode == ISA::RISCV::CUSTOM_OPCODE0)
		{

		}

		instr_info.execute(this, instr);
		_int_regs.zero.u64 = 0ull;

		if(!branch_taken) pc += 4;
		else branch_taken = false;

		if(instr_data.type == ISA::RISCV::Type::LOAD)
		{
			MemoryRequestItem request_item;
			request_item.return_buffer_id = _return_buffer.id;
			request_item.return_port = 0;
			request_item.type = MemoryRequestItem::LOAD;
			request_item.dst_reg = memory_access_data.dst_reg;
			request_item.dst_reg_file = memory_access_data.dst_reg_file;
			request_item.size = memory_access_data.size;
			request_item.sign_extend = memory_access_data.sign_extend;
			request_item.paddr = memory_access_data.vaddr; //no mmu for now
			output_buffer.push_message(&request_item, _memory_higher_buffer_id, core_id);
			_stalled = true;
		}

		if(instr_data.type == ISA::RISCV::Type::STORE)
		{
			MemoryRequestItem request_item;
			request_item.return_buffer_id = _return_buffer.id;
			request_item.return_port = 0;
			request_item.type = MemoryRequestItem::STORE;
			request_item.size = memory_access_data.size;
			request_item.paddr = memory_access_data.vaddr; //no mmu for now
			//if(memory_access_data.vaddr < 1024 * 1024 * 512) printf("Store: \n");
			std::memcpy(&request_item.data, memory_access_data.store_data_u8, std::abs(request_item.size));
			output_buffer.push_message(&request_item, _memory_higher_buffer_id, core_id);
			_stalled = true;
		}

		if(pc == 0)
		{
			printf("Done: %d\n", core_id);
			executing = false;
		}
	}
};

}}
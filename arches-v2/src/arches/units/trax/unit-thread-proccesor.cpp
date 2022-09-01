#include "unit-thread-proccesor.hpp"

namespace Arches {
	namespace Units {
		namespace TRaX {

			UnitThreadProccesor::UnitThreadProccesor(UnitMemoryBase* mem_higher, UnitMemoryBase* main_memory, UnitAtomicIncrement* atomic_inc, Simulator* simulator) :
				ExecutionBase(&_int_regs, &_float_regs),
				UnitMemoryBase(mem_higher, simulator)
			{
				ISA::RISCV::isa[ISA::RISCV::CUSTOM_OPCODE0] = ISA::RISCV::TRaX::traxamoin;

				_atomic_inc.buffer_id = atomic_inc->get_request_buffer_id();
				_atomic_inc.client_id = atomic_inc->add_client();

				_main_mem.buffer_id = main_memory->get_request_buffer_id();
				_main_mem.client_id = main_memory->add_client();

				output_buffer.resize(1, sizeof(UnitMemoryBase));
				_int_regs.ra.u64 = 0ull;

				offset_bits = mem_higher->offset_bits;
				offset_mask = mem_higher->offset_mask;

				executing = true;
			}

			void UnitThreadProccesor::acknowledge(buffer_id_t buffer)
			{
				if(buffer == _memory_higher_buffer_id)
				{
					_stalled_for_mem_ack = false;
					//we need to execute the deco
				}
			}

			void UnitThreadProccesor::execute()
			{
				if(!executing) return;
				//if(_stalled_for_store_ack) return;

				//sim wb
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
					_return_buffer.clear(return_index);

					_wb_latch_filled = true;
					wb.dst_reg = memory_access_data.dst_reg;
					wb.dst_reg_file = memory_access_data.dst_reg_file;
				}

				if(!_wb_latch_filled)
				{
					//sim return from exec unit
				}

				if(_wb_latch_filled)
				{
					//sim wb
					if(wb.dst_reg_file == 0)
					{
						int_regs->valid[wb.dst_reg] = true;
					}
					else
					{
						float_regs->valid[wb.dst_reg] = true;
					}
				}

				//decode


				//fetch


				//sim 
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

					output_buffer.push_message(&request_item, _atomic_inc.buffer_id, _atomic_inc.client_id);
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
						_stalled_for_mem_ack = true;
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

						output_buffer.push_message(&request_item, _main_mem.buffer_id, _main_mem.client_id);
						_stalled_for_mem_ack = true;
					}
				}

				if(pc == 0)
				{
					executing = false;
				}
			}
		}
	}
}
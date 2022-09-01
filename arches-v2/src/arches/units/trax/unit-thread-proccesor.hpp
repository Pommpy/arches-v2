#pragma once

#include "../../../stdafx.hpp"

#include "../unit-base.hpp"
#include "../unit-execution-base.hpp"
#include "../unit-memory-base.hpp"
#include "../uint-atomic-increment.hpp"

#include "../../isa/execution-base.hpp"
#include "../../isa/registers.hpp"


namespace Arches {
	namespace ISA {
		namespace RISCV {
			namespace TRaX {

				//TRAXAMOIN
				const static InstructionInfo traxamoin(0b00010, "traxamoin", Type::CUSTOM, Encoding::U, RegFile::INT, IMPL_DECL
					{
						unit->memory_access_data.dst_reg_file = 0;
						unit->memory_access_data.dst_reg = instr.i.rd;
						unit->memory_access_data.sign_extend = false;
						unit->memory_access_data.size = 4;
					});
			}
		}
	}
}

namespace Arches {
	namespace Units {
		namespace TRaX {

			class UnitThreadProccesor : public ISA::RISCV::ExecutionBase, public UnitMemoryBase
			{
			public:
				class Log
				{
				protected:
					uint64_t _instruction_counters[static_cast<size_t>(ISA::RISCV::Type::NUM_TYPES)];

				public:
					Log() { reset(); }

					void reset()
					{
						for(uint i = 0; i < static_cast<size_t>(ISA::RISCV::Type::NUM_TYPES); ++i)
							_instruction_counters[i] = 0;
					}

					void acculuate(const Log& other)
					{
						for(uint i = 0; i < static_cast<size_t>(ISA::RISCV::Type::NUM_TYPES); ++i)
							_instruction_counters[i] += other._instruction_counters[i];
					}

					void log_instruction(const ISA::RISCV::InstructionData& data)
					{
						_instruction_counters[static_cast<size_t>(data.type)]++;
					}

					void print_log()
					{
						uint64_t total = 0;
						for(uint i = 0; i < static_cast<size_t>(ISA::RISCV::Type::NUM_TYPES); ++i)
						{
							printf("%s: %lld\n", ISA::RISCV::instr_type_names[i].c_str(), _instruction_counters[i]);
							total += _instruction_counters[i];
						}

						printf("Total: %lld\n", total);
					}
				}log;

			private:
				ISA::RISCV::IntegerRegisterFile _int_regs{};
				ISA::RISCV::FloatingPointRegisterFile _float_regs{};

				struct _UnitInfo
				{
					buffer_id_t buffer_id;
					client_id_t client_id;
				}; 

				_UnitInfo _atomic_inc;
				_UnitInfo _main_mem;

				_UnitInfo _alu;
				_UnitInfo _int_mul;
				_UnitInfo _fp_mul_info;
				_UnitInfo _fp_add_info;
				_UnitInfo _fp_sqrt_info;
				_UnitInfo _fp_div_info;

				uint8_t _stack[1024];
				paddr_t _stack_start{0ull - sizeof(_stack)};

				bool _stalled_for_mem_ack{false};

				bool _ex_latch_filled{false};
				ExecutionItem ex_item;
				
				bool _wb_latch){false};
				WriteBackItem wb;

			public:
				//this is how you declare the size of output buffer. In this case we want to send one memory request item on any given cycle
				//also we must register our input buffers in contructor by convention
				UnitThreadProccesor(UnitMemoryBase* mem_higher, UnitMemoryBase* main_memory, UnitAtomicIncrement* atomic_inc, Simulator* simulator);

				void acknowledge(buffer_id_t buffer) override;

				void execute() override;

			private:
				void _fetch();
				void _decode();
				void _mem();
				void _wb();

			};
		}
	}
}
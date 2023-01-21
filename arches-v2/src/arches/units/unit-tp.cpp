#include "unit-tp.hpp"

namespace Arches { namespace Units {

UnitTP::UnitTP(const Configuration& config, Simulator* simulator) : log(0x10000), UnitBase(simulator), ISA::RISCV::ExecutionBase(&_int_regs, &_float_regs)
{
	int_regs->sp.u64 = config.sp;
	int_regs->ra.u64 = 0;
	pc = config.pc;

	backing_memory = config.backing_memory;

	mu_map = config.mu_map;
	sfu_table = config.sfu_table;

	tp_index = config.tp_index;
	tm_index = config.tm_index;

	executing = true;

	for(uint i = 0; i < 32; ++i)
	{
		_int_regs_pending[i] = 0;
		_float_regs_pending[i] = 0;
	}
}

void UnitTP::_clear_register_pending(const ISA::RISCV::RegAddr& dst)
{
	     if(dst.reg_file == 0) _int_regs_pending[dst.reg] = 0;
	else if(dst.reg_file == 1) _float_regs_pending[dst.reg] = 0;
}

void UnitTP::_process_load_return(const MemoryRequest& rtrn)
{
	for(uint i = 0; i < lsq.size(); ++i)
	{
		LSE& lse = lsq[i];
		//TODO not clear that we can commit multiple registers in a single cycle
		//Commit all the lse that have already issued if the havent issued we will short corcuit when we try to issue
		if(lse.state == LSE::State::ISSUED && lse.port_addr == rtrn.paddr)
		{
			if(rtrn.type == MemoryRequest::Type::AMO_RETURN) _write_register(lse.dst, lse.size, rtrn._data);
			_clear_register_pending(lse.dst);
			lse.state = LSE::State::COMMITED;
		}
	}
}

uint8_t UnitTP::_check_dependancies(const ISA::RISCV::Instruction instr, ISA::RISCV::InstructionInfo const& instr_info)
{
	uint8_t* dst_pending = (uint8_t)instr_info.dst_reg_file ? _float_regs_pending : _int_regs_pending;
	uint8_t* src_pending = (uint8_t)instr_info.src_reg_file ? _float_regs_pending : _int_regs_pending;

	switch(instr_info.encoding)
	{
	case ISA::RISCV::Encoding::R:
		if(dst_pending[instr.rd]) return dst_pending[instr.rd];
		if(src_pending[instr.rs1]) return src_pending[instr.rs1];
		if(src_pending[instr.rs2]) return src_pending[instr.rs2];
		dst_pending[instr.rd] = (uint8_t)instr_info.type;
		break;

	case ISA::RISCV::Encoding::R4:
		if(dst_pending[instr.rd]) return dst_pending[instr.rd];
		if(src_pending[instr.rs1]) return src_pending[instr.rs1];
		if(src_pending[instr.rs2]) return src_pending[instr.rs2];
		if(src_pending[instr.rs3]) return src_pending[instr.rs3];
		dst_pending[instr.rd] = (uint8_t)instr_info.type;
		break;

	case ISA::RISCV::Encoding::I:
		if(dst_pending[instr.rd]) return dst_pending[instr.rd];
		if(src_pending[instr.rs1]) return src_pending[instr.rs1];
		dst_pending[instr.rd] = (uint8_t)instr_info.type;
		break;

	case ISA::RISCV::Encoding::S:
		if(dst_pending[instr.rs2]) return dst_pending[instr.rs2];
		if(src_pending[instr.rs1]) return src_pending[instr.rs1];
		break;

	case ISA::RISCV::Encoding::B:
		if(src_pending[instr.rs1]) return src_pending[instr.rs1];
		if(src_pending[instr.rs2]) return src_pending[instr.rs2];
		break;

	case ISA::RISCV::Encoding::U:
		if(dst_pending[instr.rd]) return dst_pending[instr.rd];
		if(instr.opcode == ISA::RISCV::CUSTOM_OPCODE0)
		{
			if     (instr.u.imm == 1) for(uint i = 3; i < 17; ++i) if(_float_regs_pending[i]) return _float_regs_pending[i]; //boxisect
			else if(instr.u.imm == 2) for(uint i = 0; i < 20; ++i) if(_float_regs_pending[i]) return _float_regs_pending[i]; //triisect
			else if(instr.u.imm == 3) for(uint i = 0; i < 21; ++i) if(_float_regs_pending[i]) return _float_regs_pending[i]; //evdb
			//else if(instr.u.imm == 4) for(uint i = 0; i < 21; ++i) if(_float_regs_pending[i]) return _float_regs_pending[i]; //evdb
		}
		dst_pending[instr.rd] = (uint8_t)instr_info.type;
		break;

	case ISA::RISCV::Encoding::J:
		if(dst_pending[instr.rd]) return dst_pending[instr.rd];
		dst_pending[instr.rd] = (uint8_t)instr_info.type;
		break;
	}

	_int_regs_pending[0] = 0;
	return 0;
}


void UnitTP::clock_rise()
{
REPEAT_MU_LOOP:

	for(uint i = 0; i < lsq.size(); ++i)
	{
		LSE& lse = lsq[i];
		if(lse.state != LSE::State::ISSUED) continue;

		//check for returned LOAD
		UnitMemoryBase* unit = mu_map->get_unit(lse.unit_index);
		uint index = mu_map->get_offset(lse.unit_index) + tp_index;
		if(!unit->return_bus.get_pending(index)) continue;

		_process_load_return(unit->return_bus.get_data(index));
		unit->return_bus.clear_pending(index);
	}

	//TODO make this less janky
REPEAT_SFU_LOOP:
	for(auto& a : _issued_sfus)
	{
		if(a.first->return_bus.get_pending(tp_index))
		{
			UnitSFU::Request return_item = a.first->return_bus.get_data(tp_index);
			a.first->return_bus.clear_pending(tp_index);

			_clear_register_pending(return_item.dst);

			if(--a.second == 0)
			{
				_issued_sfus.erase(a.first);
				goto REPEAT_SFU_LOOP;
			}
		}
	}
}

#define MAGIC_COMPUTE

void UnitTP::clock_fall()
{
FREE_INSTR:
	if(pc == 0ull) executing = false;
	if(!executing) return;

	//if the instruction wasn't accepted by the sfu we must stall
	if(_last_issue_sfu)
	{
		if(_last_issue_sfu->request_bus.get_pending(tp_index))
		{
			log.log_resource_stall(_last_instr_info);
			goto DRAIN_LSQ;
		}
		else if(_last_issue_sfu->latency == 1) //simulate forwarding on single cycle instructions
		{
			if((uint8_t)_last_instr_info.dst_reg_file == 0) _int_regs_pending[_last_instr.rd] = 0;
			else if((uint8_t)_last_instr_info.dst_reg_file == 1) _float_regs_pending[_last_instr.rd] = 0;
		}
	}
	_last_issue_sfu = nullptr;

	//if we are stalled for LSQ we need to stall on the last mem instr
	if(!stalled_for_lsq)
	{
		//fetch
		ISA::RISCV::Instruction instr(reinterpret_cast<uint32_t*>(backing_memory)[pc / 4]);
		const ISA::RISCV::InstructionInfo instr_info = instr.get_info();

		//if we don't have the dependecy we have to stall.
		if(uint8_t type = _check_dependancies(instr, instr_info))
		{
			log.log_data_stall(type);
			goto DRAIN_LSQ;
		}

		log.log_instruction_issue(instr_info, pc);

	#if 0
		if(tp_index == 0 && tm_index == 0)
		{
			printf("    %05I64x: \t%08x          \t", pc, instr.data);
			instr_info.print_instr(instr);
			printf("\n");
		}
	#endif

		instr_info.execute(this, instr); _int_regs.zero.u64 = 0ull;

		_last_instr = instr;
		_last_instr_info = instr_info;

		if(!branch_taken) pc += 4;
		else branch_taken = false;
	}

	if(_last_instr_info.type <= ISA::RISCV::Type::AMO) //AMO is the last memory instruction type
	{
		//create LSE
		uint unit_index = mu_map->get_index(mem_req.vaddr);
		if(mu_map->get_unit(unit_index))
		{
			LSE& lse = lsq[next_lse];
			if(lse.state == LSE::State::COMMITED || lse.state == LSE::State::FREE)
			{
				//if the next entry in the circular buffer is free or commited we can reuse it
				paddr_t paddr = mem_req.vaddr;
				if(mem_req.type == MemoryRequest::Type::LOAD) lse.port_addr = _get_load_port_address(paddr);
				else lse.port_addr = paddr;
				lse.offset = paddr - lse.port_addr;
				lse.size = mem_req.size;

				lse.type = mem_req.type;
				lse.dst = mem_req.dst;
				lse.unit_index = unit_index;

				lse.state = LSE::State::VALID;

				//if this is a load we need to alight it to the port width(32bytes)
				
				next_lse = (next_lse + 1) % lsq.size();

				stalled_for_lsq = false;
			}
			else
			{
				//stall since LSE is full
				log.log_resource_stall(_last_instr_info);
				stalled_for_lsq = true;
			}
		}
		else
		{
			if(_last_instr_info.encoding == ISA::RISCV::Encoding::I)
			{
				//because of forwarding instruction with latency 1 don't cause stalls so we don't need to simulate
				_clear_register_pending(mem_req.dst);
			}
		#ifdef MAGIC_COMPUTE
			goto FREE_INSTR;
		#endif
		}
	}
	else
	{
		//issue to SFU
		UnitSFU::Request request;
		request.dst.reg = _last_instr.rd;
		request.dst.reg_file = (uint8_t)_last_instr_info.dst_reg_file;

	#ifdef MAGIC_COMPUTE
		_last_issue_sfu = nullptr;  
	#else 
		_last_issue_sfu = sfu_table[static_cast<uint>(_last_instr_info.type)];
	#endif

		if(_last_issue_sfu)
		{
			_last_issue_sfu->request_bus.set_data(request, tp_index);
			_last_issue_sfu->request_bus.set_pending(tp_index);
			_issued_sfus[_last_issue_sfu]++;
		}
		else
		{
			//because of forwarding instruction with latency 1 don't cause stalls so we don't need to simulate
			_clear_register_pending(request.dst);
		#ifdef MAGIC_COMPUTE
			goto FREE_INSTR;
		#endif
		}
	}


DRAIN_LSQ:
	//try to issue next entry from LSQ
	//if issue LSE == next LSE then we have drained the queue
	while(lsq[oldest_lse].state == LSE::State::VALID)
	{ 
		//if we havent issued or commited check to see if another entry commited or issued the same address
		//otherwise we can go ahead and increment oldent index since this is now 
		LSE& lse = lsq[oldest_lse];
		LSE::State highest_exiting_state = LSE::State::VALID;
		for(uint i = 0; i < lsq.size(); ++i)
			if(lsq[i].port_addr == lse.port_addr)
				highest_exiting_state = std::max(highest_exiting_state, lsq[i].state);

		if(highest_exiting_state == LSE::State::COMMITED)
		{
			//TODO not sure if we can commit multiple registers on a single cycle
			//antoher entry has what we need so we can just commit the register?
			lse.state = LSE::State::COMMITED;
			_clear_register_pending(lse.dst);
		}
		else if(highest_exiting_state == LSE::State::ISSUED)
		{
			//we have already issued this request so we can mark as issued
			lse.state = LSE::State::ISSUED;
		}
		else //VALID
		{
			//not issued or in the queue so we need to fetch it
			UnitMemoryBase* unit = mu_map->get_unit(lse.unit_index);
			uint index = mu_map->get_offset(lse.unit_index) + tp_index;
			if(unit->request_bus.get_pending(index)) break; //for now issue in order

			MemoryRequest request;
			request.paddr = lse.port_addr;
			request.type = lse.type;
			
			if(lse.type == MemoryRequest::Type::LOAD) request.size = 32;
			else request.size = lse.size;

			unit->request_bus.set_data(request, index);
			unit->request_bus.set_pending(index);

			if(lse.type == MemoryRequest::Type::STORE) lse.state = LSE::State::COMMITED;
			else                                       lse.state = LSE::State::ISSUED;
		}

		oldest_lse = (oldest_lse + 1) % lsq.size();
	} 
}

}}
#include "unit-tp.hpp"

namespace Arches { namespace Units {

//#define LOGGING_ENABLED (_tp_index == 0 && _tm_index == 0)
#define LOGGING_ENABLED (false)

UnitTP::UnitTP(const Configuration& config) : log(0x10000), ISA::RISCV::ExecutionBase(&_int_regs, &_float_regs)
{
	int_regs->sp.u64 = config.sp;
	int_regs->ra.u64 = 0x0ull;
	pc = config.pc;

	backing_memory = config.backing_memory;

	mem_map = config.mem_map;

	sfu_table = config.sfu_table;

	_tp_index = config.tp_index;
	_tm_index = config.tm_index;

	_stack_mem.resize(config.stack_size);
	_stack_mask = generate_nbit_mask(log2i(config.stack_size));

	_port_size = config.port_size;
	_port_mask = ~generate_nbit_mask(log2i(config.port_size));

	for(uint i = 0; i < 32; ++i)
	{
		_int_regs_pending[i] = 0;
		_float_regs_pending[i] = 0;
	}
}

void UnitTP::_clear_register_pending(const ISA::RISCV::RegAddr& dst)
{
	     if(dst.reg_type == 0) _int_regs_pending[dst.reg] = 0;
	else if(dst.reg_type == 1) _float_regs_pending[dst.reg] = 0;
}

uint8_t UnitTP::_check_dependancies(const ISA::RISCV::Instruction instr, ISA::RISCV::InstructionInfo const& instr_info)
{
	uint8_t* dst_pending = (uint8_t)instr_info.dst_reg_type ? _float_regs_pending : _int_regs_pending;
	uint8_t* src_pending = (uint8_t)instr_info.src_reg_type ? _float_regs_pending : _int_regs_pending;

	if(instr.opcode == ISA::RISCV::CUSTOM_OPCODE0)
	{
		if(instr_info.type == ISA::RISCV::Type::BOXISECT) for(uint i = 0; i < 12; ++i) if(_float_regs_pending[i]) return _float_regs_pending[i]; //boxisect
		else if(instr_info.type == ISA::RISCV::Type::TRIISECT) for(uint i = 0; i < 18; ++i) if(_float_regs_pending[i]) return _float_regs_pending[i]; //triisect
		else if(instr_info.type == ISA::RISCV::Type::LBRAY) for(uint i = 0; i < 8; ++i) if(_float_regs_pending[i]) return _float_regs_pending[i]; //triisect
		else if(instr_info.type == ISA::RISCV::Type::SBRAY) for(uint i = 0; i < 6; ++i) if(_float_regs_pending[i]) return _float_regs_pending[i]; //triisect
		else if(instr_info.type == ISA::RISCV::Type::CSHIT) for(uint i = 15; i < 19; ++i) if(_float_regs_pending[i]) return _float_regs_pending[i]; //triisect
	}

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

void UnitTP::_process_load_return(const MemoryReturn& ret)
{
	if(LOGGING_ENABLED) printf("LOAD RETURN(%d):%lld\n", _tp_index, ret.vaddr);
	_write_register(ret.dst, ((uint8_t*)ret.data), ret.size);
	_clear_register_pending(ret.dst);
}

void UnitTP::_try_drain_lsq()
{
	if(_lsq.empty()) return;

	const MemoryRequest& req = _lsq.front();

	//Not issued or in the queue so we need to fetch it
	uint mappig_index = mem_map.get_mapping_index(req.vaddr);
	const MemoryMap::MemoryMapping& mapping = mem_map.mappings[mappig_index];
	if(!mapping.unit->request_port_write_valid(mapping.port_index)) return; //For now issue requests in order

	if(LOGGING_ENABLED && req.type == MemoryRequest::Type::LOAD) printf("LOAD(%d):%lld\n",_tp_index, req.vaddr);
	mapping.unit->write_request(req, mapping.port_index);
	_lsq.pop();
}

void UnitTP::clock_rise()
{
	for(uint i = 0; i < mem_map.mappings.size(); ++i)
	{
		MemoryMap::MemoryMapping mapping = mem_map.mappings[i];
		for(uint j = 0; j < mapping.num_ports; ++j, ++mapping.port_index, ++mapping.port_id)
		{
			if(!mapping.unit->return_port_read_valid(mapping.port_index)) continue;
			const MemoryReturn& ret = mapping.unit->read_return(mapping.port_index);
			_process_load_return(ret);
		}
	}

REPEAT_SFU_LOOP: //TODO make this less janky
	for(auto& a : _issued_sfus)
	{
		if(a.first->return_bus.transaction_pending(_tp_index))
		{
			UnitSFU::Request return_item = a.first->return_bus.get_transaction(_tp_index);
			a.first->return_bus.acknowlege(_tp_index);

			_clear_register_pending(return_item.dst);

			if(--a.second == 0)
			{
				_issued_sfus.erase(a.first);
				goto REPEAT_SFU_LOOP;
			}
		}
	}
}

//#define MAGIC_COMPUTE

void UnitTP::clock_fall()
{
FREE_INSTR:
	if(pc == 0x0ull) return;

	//If the instruction wasn't accepted by the sfu we must stall
	if(_last_issue_sfu)
	{
		if(_last_issue_sfu->request_bus.transaction_pending(_tp_index))
		{
			log.log_resource_stall(_last_instr_info);
			_try_drain_lsq();
			return;
		}
		else if(_last_issue_sfu->latency == 1) //Simulate forwarding on single cycle instructions
		{
			if((uint8_t)_last_instr_info.dst_reg_type == 0) _int_regs_pending[_last_instr.rd] = 0;
			else if((uint8_t)_last_instr_info.dst_reg_type == 1) _float_regs_pending[_last_instr.rd] = 0;
		}
	}
	_last_issue_sfu = nullptr;

	//If we are stalled for LSQ we need to stall on the last mem instr
	if(!_stalled_for_lsq)
	{
		//Fetch
		ISA::RISCV::Instruction instr(reinterpret_cast<uint32_t*>(backing_memory)[pc / 4]);
		const ISA::RISCV::InstructionInfo instr_info = instr.get_info();

		//If we don't have the dependecy we have to stall.
		if(uint8_t type = _check_dependancies(instr, instr_info))
		{
			log.log_data_stall(type);
			_try_drain_lsq();
			return;
		}

		log.log_instruction_issue(instr_info, pc);

	#if 1
		if(LOGGING_ENABLED)
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
		else
		{
			//jumping to address 0 is the halt condition
			if(pc == 0x0) simulator->units_executing--;
			branch_taken = false;
		}
	}

	if(_last_instr_info.type < ISA::RISCV::Type::SYS) //SYS is the first non memory instruction type so this divides mem and non mem ops
	{
		//Create LSE
		uint mappig_index = mem_map.get_mapping_index(mem_req.vaddr);
		if(mappig_index != ~0u)
		{
			//If the the lsq is not full add the request to it
			if(true)
			{
				_lsq.push(mem_req);
			}
			else
			{
				//stall since LSQ is full
				log.log_resource_stall(_last_instr_info);
				_stalled_for_lsq = true;
			}
		}
		else
		{
			if(_last_instr_info.type == ISA::RISCV::Type::LOAD)
			{
				//Because of forwarding instruction with latency 1 don't cause stalls so we don't need to simulate
				paddr_t buffer_addr = mem_req.vaddr & _stack_mask;
				_write_register(mem_req.dst, &_stack_mem[buffer_addr], mem_req.size);
				_clear_register_pending(mem_req.dst);
			}
			else if(_last_instr_info.type == ISA::RISCV::Type::STORE)
			{
				paddr_t buffer_addr = mem_req.vaddr & _stack_mask;
				std::memcpy(&_stack_mem[buffer_addr], mem_req.data, mem_req.size);
			}
			else
			{
				assert(false);
			}
		#ifdef MAGIC_COMPUTE
			goto FREE_INSTR;
		#endif
		}
	}
	else
	{
		//Issue to SFU
		UnitSFU::Request request;
		request.dst.reg = _last_instr.rd;
		request.dst.reg_type = (uint8_t)_last_instr_info.dst_reg_type;

	#ifdef MAGIC_COMPUTE
		_last_issue_sfu = nullptr;  
	#else 
		_last_issue_sfu = sfu_table[static_cast<uint>(_last_instr_info.type)];
	#endif

		if(_last_issue_sfu)
		{
			_last_issue_sfu->request_bus.add_transaction(request, _tp_index);
			_issued_sfus[_last_issue_sfu]++;
		}
		else
		{
			//Because of forwarding instruction with latency 1 don't cause stalls so we don't need to simulate
			_clear_register_pending(request.dst);
		#ifdef MAGIC_COMPUTE
			goto FREE_INSTR;
		#endif
		}
	}

	_try_drain_lsq();
}

}}
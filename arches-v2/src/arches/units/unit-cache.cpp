#include "unit-cache.hpp"

namespace Arches {namespace Units {

UnitCache::UnitCache(Configuration config) : UnitMemoryBase(config.num_ports),
	incoming_request_network(config.num_ports, config.num_banks),
	outgoing_return_network(config.num_banks, config.num_ports),
	outgoing_request_network(config.num_banks, config.mem_map.total_ports),
	incoming_return_network(config.mem_map.total_ports, config.num_banks)
{
	_mem_map = config.mem_map;

	_configuration = config;

	_tag_array.resize(config.size / CACHE_BLOCK_SIZE);
	_data_array.resize(config.size / CACHE_BLOCK_SIZE);
	_banks.resize(config.num_banks, {config.num_lfb});

	_associativity = config.associativity;
	_penalty = config.penalty;

	uint num_sets = config.size / (CACHE_BLOCK_SIZE * config.associativity);

	uint offset_bits = log2i(CACHE_BLOCK_SIZE);
	uint set_index_bits = log2i(num_sets);
	uint tag_bits = static_cast<uint>(sizeof(paddr_t) * 8) - (set_index_bits + offset_bits);
	uint bank_index_bits = log2i(config.num_banks);

	_block_offset_mask = generate_nbit_mask(offset_bits);
	_set_index_mask = generate_nbit_mask(set_index_bits);
	_tag_mask = generate_nbit_mask(tag_bits);
	_bank_index_mask = generate_nbit_mask(bank_index_bits);

	_set_index_offset = offset_bits;
	_tag_offset = offset_bits + set_index_bits;
	_bank_index_offset = log2i(CACHE_BLOCK_SIZE); //line stride for now
	_port_width = _configuration.port_size;

	_pwords_per_block = CACHE_BLOCK_SIZE / _port_width;
}

UnitCache::~UnitCache()
{

}

uint UnitCache::_fetch_lfb(uint bank_index, _LFB& lfb)
{
	_Bank& bank = _banks[bank_index];
	std::vector<_LFB>& lfbs = bank.lfbs;

	for(uint32_t i = 0; i < lfbs.size(); ++i)
		if(lfbs[i].state != _LFB::State::INVALID && lfbs[i] == lfb) return i;

	return ~0u;
}

uint UnitCache::_allocate_lfb(uint bank_index, _LFB& lfb)
{
	std::vector<_LFB>& lfbs = _banks[bank_index].lfbs;

	uint replacement_index = ~0u;
	uint replacement_lru = 0u;
	for(uint i = 0; i < lfbs.size(); ++i)
	{
		if(lfbs[i].state == _LFB::State::INVALID)
		{
			replacement_index = i;
			break;
		}

		if(lfbs[i].state == _LFB::State::FILLED && lfbs[i].commited && lfbs[i].returned && lfbs[i].lru >= replacement_lru)
		{
			replacement_lru = lfbs[i].lru;
			replacement_index = i;
		}
	}

	//can't allocate
	if(replacement_index == ~0) return ~0;

	for(uint i = 0; i < lfbs.size(); ++i) lfbs[i].lru++;
	lfbs[replacement_index].lru = 0;
	lfbs[replacement_index] = lfb;
	return replacement_index;
}

uint UnitCache::_fetch_or_allocate_lfb(uint bank_index, uint64_t block_addr, _LFB::Type type)
{
	std::vector<_LFB>& lfbs = _banks[bank_index].lfbs;

	_LFB lfb;
	lfb.block_addr = block_addr;
	lfb.type = type;
	lfb.state = _LFB::State::EMPTY;
	
	uint lfb_index = _fetch_lfb(bank_index, lfb);
	if(lfb_index != ~0) return lfb_index;
	return _allocate_lfb(bank_index, lfb);

}

//update lru and returns data pointer to cache line
UnitCache::_BlockData* UnitCache::_get_block(paddr_t paddr)
{
	uint start = _get_set_index(paddr) * _associativity;
	uint end = start + _associativity;

	uint64_t tag = _get_tag(paddr);

	uint found_index = ~0;
	uint found_lru = 0;
	for(uint i = start; i < end; ++i)
	{
		if(_tag_array[i].valid && _tag_array[i].tag == tag)
		{
			found_index = i;
			found_lru = _tag_array[i].lru;
			break;
		}
	}

	if(found_index == ~0) return nullptr; //didn't find line so we will leave lru alone and return nullptr

	for(uint i = start; i < end; ++i)
		if(_tag_array[i].lru < found_lru) _tag_array[i].lru++;

	_tag_array[found_index].lru = 0;

	return &_data_array[found_index];
}

//inserts cacheline associated with paddr replacing least recently used. Assumes cachline isn't already in cache if it is this has undefined behaviour
UnitCache::_BlockData* UnitCache::_insert_block(paddr_t paddr, _BlockData& data)
{
	uint start = _get_set_index(paddr) * _associativity;
	uint end = start + _associativity;

	uint replacement_index = ~0u;
	uint replacement_lru = 0u;
	for(uint i = start; i < end; ++i)
	{
		if(!_tag_array[i].valid)
		{
			replacement_index = i;
			break;
		}
		
		if(_tag_array[i].lru >= replacement_lru)
		{
			replacement_lru = _tag_array[i].lru;
			replacement_index = i;
		}
	}

	for(uint i = start; i < end; ++i)
		_tag_array[i].lru++;

	_tag_array[replacement_index].lru = 0;
	_tag_array[replacement_index].valid = true;
	_tag_array[replacement_index].tag = _get_tag(paddr);

	_data_array[replacement_index] = data;
	return &_data_array[replacement_index];
}

void UnitCache::_update_interconection_network_rise()
{
	//route ports to arbitrators
	for(uint mapping_index = 0; mapping_index < _mem_map.mappings.size(); ++mapping_index)
	{
		MemoryUnitMap::MemoryUnitMapping mapping = _mem_map.mappings[mapping_index];
		for(uint i = 0; i < mapping.num_ports; ++i)
		{
			if(!mapping.unit->return_bus.transfer_pending(mapping.port_index + i)) continue;

			//if there is pending request try map it to to the correct bank
			const MemoryRequest& request = mapping.unit->return_bus.transfer(mapping.port_index + i);
			uint port_id = mapping.port_id + i;
			uint bank_index = _get_bank_index(request.paddr);
			incoming_return_network.add(port_id, bank_index);
		}
	}

	for(uint port_index = 0; port_index < request_bus.size(); ++port_index)
	{
		if(!request_bus.transfer_pending(port_index)) continue;

		const MemoryRequest& request = request_bus.transfer(port_index);
		uint32_t bank_index = _get_bank_index(request.paddr);
		incoming_request_network.add(port_index, bank_index);
	}

	//update incoming arbitrators
	incoming_request_network.update();
	incoming_return_network.update();
}

void UnitCache::_proccess_returns(uint bank_index)
{
	uint port_id = incoming_return_network.input(bank_index);
	if(port_id == ~0) return;

	uint mapping_index = _mem_map.get_mapping_index_for_unique_port_index(port_id);
	MemoryUnitMap::MemoryUnitMapping mapping = _mem_map.mappings[mapping_index];
	uint port_index = mapping.port_index + (port_id - mapping.port_id);

	const MemoryRequest& rtrn = mapping.unit->return_bus.transfer(port_index);
	assert(rtrn.paddr == _get_block_addr(rtrn.paddr));

	//mark all of the associated lse as ready to return
	_Bank& bank = _banks[bank_index];
	for(uint i = 0; i < bank.lfbs.size(); ++i)
		if(bank.lfbs[i].block_addr == rtrn.paddr)
		{ 
			bank.lfbs[i].state = _LFB::State::FILLED;
			bank.lfbs[i].block_data = *((_BlockData*)rtrn.data);
		}

	mapping.unit->return_bus.acknowlege(port_index);
	incoming_return_network.remove(port_id, bank_index);
}

void UnitCache::_proccess_requests(uint bank_index)
{
	_Bank& bank = _banks[bank_index];
	bank.outgoing_return_new_request_mask = 0x0ull;

	uint port_index = incoming_request_network.input(bank_index);
	if(port_index == ~0) return;

	const MemoryRequest& request = request_bus.transfer(port_index);
	paddr_t block_addr = _get_block_addr(request.paddr);
	uint block_offset = _get_block_offset(request.paddr);

	if(request.type == MemoryRequest::Type::LOAD)
	{
		//try to allocate an lse for one incoming requests
		//if the request is aready in an lse then merge
		//if there is another identical request on a diffrent port merge that into the lse aswell
		uint lfb_index = _fetch_or_allocate_lfb(bank_index, block_addr, _LFB::Type::READ);
		if(lfb_index != ~0)
		{
			assert(request.size == _port_width);
			uint64_t request_mask = 0x0ull;

			//merge identical requests
			for(uint merge_port_index = 0; merge_port_index < request_bus.size(); ++merge_port_index)
			{
				if(!request_bus.transfer_pending(merge_port_index)) continue;

				if(request_bus.transfer(merge_port_index) == request)
				{
					request_mask |= 0x1ull << merge_port_index;
					request_bus.acknowlege(merge_port_index);
					incoming_request_network.remove(merge_port_index, bank_index);
				}
			}

			uint pword_index = block_offset / _port_width;
			_LFB& lfb = bank.lfbs[lfb_index];
			lfb.returned = false; //since we are adding new
			if(lfb_index == bank.outgoing_return_lfb && pword_index == bank.outgoing_return_pword_index) 
				bank.outgoing_return_new_request_mask = request_mask;
			else lfb.pword_request_masks[pword_index] |= request_mask;

			//TODO: should we log the each merge request seperatly?
			if(lfb.state == _LFB::State::MISSED || lfb.state == _LFB::State::ISSUED) log.log_half_miss();
			if(lfb.state == _LFB::State::FILLED) log.log_hit();
		}
		else log.log_lfb_stall();
	}
	else if(request.type == MemoryRequest::Type::STORE) //place stores directly in WC. since they are uncached we dont need to aquire the bank
	{
		uint lfb_index = _fetch_or_allocate_lfb(bank_index, block_addr, _LFB::Type::WRITE_COMBINING);
		if(lfb_index != ~0)
		{
			//add to lfb
			_LFB& lfb = bank.lfbs[lfb_index];
			lfb.state = _LFB::State::FILLED;
			lfb.block_addr = block_addr;
			lfb.byte_mask |= generate_nbit_mask(request.size) << block_offset;
			std::memcpy(&lfb.block_data.bytes[block_offset], request.data, request.size);

			request_bus.acknowlege(port_index);
			incoming_request_network.remove(port_index, bank_index);
		}
	}

	if(incoming_request_network.inputs_pending(bank_index) > 0) log.log_bank_conflict(); //log bank conflicts
}

void UnitCache::_propagate_acknowledge_signals()
{
	//outgoing returns
	for(uint port_index = 0; port_index < return_bus.size(); ++port_index)
	{
		uint bank_index = outgoing_return_network.input(port_index);
		if(bank_index == ~0 || return_bus.transfer_pending(port_index)) continue;

		outgoing_return_network.remove(bank_index, port_index);

		//Remove the request from the mask
		_Bank& bank = _banks[bank_index];
		assert(bank.outgoing_return_lfb != ~0u);

		_LFB& lfb = bank.lfbs[bank.outgoing_return_lfb];
		lfb.pword_request_masks[bank.outgoing_return_pword_index] &= ~(0x1ull << port_index);
	}

	//outgoing requests
	for(uint mapping_index = 0; mapping_index < _mem_map.mappings.size(); ++mapping_index)
	{
		MemoryUnitMap::MemoryUnitMapping mapping = _mem_map.mappings[mapping_index];
		for(uint i = 0; i < mapping.num_ports; ++i)
		{
			uint port_index = mapping.port_index + i;
			uint port_id = mapping.port_id + i;
			uint bank_index = outgoing_request_network.input(port_id);
			if(bank_index == ~0 || mapping.unit->request_bus.transfer_pending(port_index)) continue;

			_Bank& bank = _banks[bank_index];
			assert(bank.outgoing_request_lfb != ~0u);

			_LFB& lfb = bank.lfbs[bank.outgoing_request_lfb];
			if(lfb.type == _LFB::Type::READ)
			{
				lfb.state = _LFB::State::ISSUED;
				bank.outgoing_request_lfb = ~0;
				outgoing_request_network.remove(bank_index, port_id);
			}
			else if(lfb.type == _LFB::Type::WRITE_COMBINING)
			{
				//clear the bytes we sent from the byte mask
				lfb.byte_mask &= ~bank.outgoing_request_byte_mask;
				if(!lfb.byte_mask)
				{
					lfb.state = _LFB::State::INVALID; //invalidate when we are done
					bank.outgoing_request_lfb = ~0;
					outgoing_request_network.remove(bank_index, port_id); //only clear if we are finished using the bus
				}
			}
		}
	}
}


void UnitCache::_update_bank(uint bank_index)
{
	_Bank& bank = _banks[bank_index];

	//advance oldest lfb to next valid entry
	if(bank.cycles_remaining == 0)
	{
		//Find the next lfb that needs to access the cache.
		//FIllED means we recived the line form mem higher and it needs to be commited
		//EMPTY means we havent checked the tag array yet
		uint32_t index = ~0u;
		uint32_t lru = 0u;
		for(uint32_t i = 0; i < bank.lfbs.size(); ++i)
		{
			_LFB& lfb = bank.lfbs[i];
			if(lfb.type == _LFB::Type::READ && 
				(lfb.state == _LFB::State::EMPTY || lfb.state == _LFB::State::FILLED) && 
				!lfb.commited && lfb.lru >= lru)
			{
				lru = lfb.lru;
				index = i;
			}
		}

		if(index != ~0)
		{
			bank.lfb_accessing_cache = index;
			bank.cycles_remaining = _penalty;
		}
	}
	if(bank.cycles_remaining == 0) return; //no valid lfb selected

	if(--bank.cycles_remaining != 0) return; //request still accessing tag array

	_LFB& lfb = bank.lfbs[bank.lfb_accessing_cache];
	if(lfb.state == _LFB::State::EMPTY)
	{
		//Check if we have the block
		_BlockData* block_data = _get_block(lfb.block_addr);
		if(block_data)
		{
			//copy data to the LFB and mark as commited
			lfb.block_data = *block_data;
			lfb.state = _LFB::State::FILLED;
			lfb.commited = true;
			log.log_hit();
		}
		else
		{
			lfb.state = _LFB::State::MISSED;
			log.log_miss();
		}
	}
	else if(lfb.state == _LFB::State::FILLED)
	{
		//insert block
		_insert_block(lfb.block_addr, lfb.block_data);
		lfb.commited = true;
	}
}

void UnitCache::_try_issue_lfb(uint bank_index)
{
	_Bank& bank = _banks[bank_index];
	if(bank.outgoing_request_lfb != ~0u) return;

	uint32_t outgoing_request_lru = 0u;
	for(uint32_t i = 0; i < bank.lfbs.size(); ++i)
	{
		_LFB& lfb = bank.lfbs[i];
		if((lfb.type == _LFB::Type::READ && lfb.state == _LFB::State::MISSED) ||
		   (lfb.type == _LFB::Type::WRITE_COMBINING && lfb.state == _LFB::State::FILLED))
		{
			outgoing_request_lru = lfb.lru;
			bank.outgoing_request_lfb = i;
		}
	}

	if(bank.outgoing_request_lfb == ~0) return;

	_LFB& lfb = bank.lfbs[bank.outgoing_request_lfb];

	uint mapping_index = _mem_map.get_mapping_index(lfb.block_addr);
	MemoryUnitMap::MemoryUnitMapping mapping = _mem_map.mappings[mapping_index];
	uint port_id = mapping.port_id + (bank_index % mapping.num_ports);
	outgoing_request_network.add(bank_index, port_id);
}

void UnitCache::_try_return_lfb(uint bank_index)
{
	_Bank& bank = _banks[bank_index];

	if(bank.outgoing_return_lfb != ~0u)
	{
		_LFB& lfb = bank.lfbs[bank.outgoing_return_lfb];

		//add new requests
		lfb.pword_request_masks[bank.outgoing_return_pword_index] |= bank.outgoing_return_new_request_mask;

		//if we haven't returnd the last item fully then we will keep this active till we do
		if(lfb.pword_request_masks[bank.outgoing_return_pword_index])
		{
			//copy requests to arbitrator
			for(uint port_index = 0; port_index < return_bus.size(); ++port_index)
				if((lfb.pword_request_masks[bank.outgoing_return_pword_index] >> port_index) & 0x1ull)
					outgoing_return_network.add(bank_index, port_index);

			return;
		}

		//otherwise check if this was the last word. If so mark the lfb as returned
		uint i = 0;
		for(i = 0; i < _pwords_per_block; ++i)
			if(lfb.pword_request_masks[i]) break;
		if(i >= _pwords_per_block) lfb.returned = true;

		//clear the lfb so we can select a new one
		bank.outgoing_return_lfb = ~0u;
		bank.outgoing_return_pword_index = 0;
	}

	//try to select a new lfb
	if(bank.outgoing_return_lfb == ~0u)
	{
		uint32_t outgoing_return_lru = 0u;
		for(uint32_t i = 0; i < bank.lfbs.size(); ++i)
		{
			_LFB& lfb = bank.lfbs[i];
			if(lfb.type == _LFB::Type::READ && lfb.state == _LFB::State::FILLED && !lfb.returned && lfb.lru >= outgoing_return_lru)
			{
				outgoing_return_lru = lfb.lru;
				bank.outgoing_return_lfb = i;
			}
		}
	}

	//this means we selected a new lfb
	if(bank.outgoing_return_lfb != ~0u)
	{
		_LFB& lfb = bank.lfbs[bank.outgoing_return_lfb];

		//select lowest pending pword 
		for(bank.outgoing_return_pword_index = 0; bank.outgoing_return_pword_index < _pwords_per_block; ++bank.outgoing_return_pword_index)
			if(lfb.pword_request_masks[bank.outgoing_return_pword_index]) break;

		//copy requests to arbitrator
		for(uint port_index = 0; port_index < return_bus.size(); ++port_index)
		{
			if((lfb.pword_request_masks[bank.outgoing_return_pword_index] >> port_index) & 0x1ull)
				outgoing_return_network.add(bank_index, port_index);
		}
	}
}

void UnitCache::_update_interconection_network_fall()
{
	//update outgoing arbitrators
	outgoing_request_network.update();
	outgoing_return_network.update();

	//copy requests from lfb
	for(uint mapping_index = 0; mapping_index < _mem_map.mappings.size(); ++mapping_index)
	{
		MemoryUnitMap::MemoryUnitMapping mapping = _mem_map.mappings[mapping_index];
		for(uint i = 0; i < mapping.num_ports; ++i)
		{
			uint port_index = mapping.port_index + i;
			uint port_id = mapping.port_id + i;
			uint bank_index = outgoing_request_network.input(port_id);
			if(bank_index == ~0u || mapping.unit->request_bus.transfer_pending(port_index)) continue; //either nothing is pending or we already issued

			//add transfer
			_Bank& bank = _banks[bank_index];
			assert(bank.outgoing_request_lfb != ~0u);

			_LFB& lfb = bank.lfbs[bank.outgoing_request_lfb];
			MemoryRequest outgoing_request;
			if(lfb.type == _LFB::Type::READ)
			{
				outgoing_request.size = CACHE_BLOCK_SIZE;
				outgoing_request.paddr = lfb.block_addr;
				outgoing_request.type = MemoryRequest::Type::LOAD;
			}
			else if(lfb.type == _LFB::Type::WRITE_COMBINING)
			{
				uint8_t offset, size;
				lfb.get_next(offset, size);
				outgoing_request.size = size;
				outgoing_request.paddr = lfb.block_addr + offset;
				outgoing_request.data = &lfb.block_data.bytes[offset];
				outgoing_request.type = MemoryRequest::Type::STORE;
				bank.outgoing_request_byte_mask = generate_nbit_mask(size) << offset;
				//todo if our transfer is pending and we have new data in the bufffer we should update it
			}

			mapping.unit->request_bus.add_transfer(outgoing_request, port_index);
		}
	}

	//copy returns from lfb
	for(uint port_index = 0; port_index < return_bus.size(); ++port_index)
	{
		uint bank_index = outgoing_return_network.input(port_index);
		if(bank_index == ~0 || return_bus.transfer_pending(port_index)) continue;
		
		_Bank& bank = _banks[bank_index];
		assert(bank.outgoing_return_lfb != ~0u);

		//add transfer
		_LFB& lfb = bank.lfbs[bank.outgoing_return_lfb];
		MemoryRequest outgoing_return;
		uint offset = bank.outgoing_return_pword_index * _port_width;
		outgoing_return.size = _port_width;
		outgoing_return.paddr = lfb.block_addr + offset;
		outgoing_return.data = &lfb.block_data.bytes[offset];
		outgoing_return.type = MemoryRequest::Type::LOAD_RETURN;
		return_bus.add_transfer(outgoing_return, port_index);
	}
}

void UnitCache::clock_rise()
{
	_update_interconection_network_rise();

	for(uint i = 0; i < _banks.size(); ++i)
	{
		_proccess_returns(i);
		_proccess_requests(i);
	}
}

void UnitCache::clock_fall()
{
	_propagate_acknowledge_signals();

	for(uint i = 0; i < _banks.size(); ++i)
	{
		_update_bank(i);
		_try_issue_lfb(i);
		_try_return_lfb(i);
	}

	_update_interconection_network_fall();
}

}}
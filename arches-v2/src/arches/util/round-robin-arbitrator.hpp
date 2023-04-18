#pragma once 
#include "../../stdafx.hpp"


//todo this is not a good implmnetation
class RoundRobinArbitrator
{
public:
	std::vector<uint8_t> pending;
	uint current_index{0};

public:
	RoundRobinArbitrator(uint size) : pending(size, 0)
	{

	}

	bool empty()
	{
		for(uint i = 0; i < pending.size(); ++i)
			if(pending[i]) return false;
		return true;
	}

	void add(uint index)
	{
		pending[index] = 1;
	}

	void remove(uint index)
	{
		pending[index] = 0;
	}

	uint next()
	{
		uint index = current_index;
		do
		{
			if(pending[index])
			{
				current_index = index + 1;
				if(current_index == pending.size()) current_index = 0;
				return index;
			}
			
			if(++index == pending.size()) index = 0;
		} 
		while(index != current_index);

		return ~0u;
	}

	uint last()
	{
		if(current_index == 0) return pending.size() - 1;
		return current_index - 1;
	}
};


//uses a 64bit integer and BM2 extension to implement a computational and stoarge efficent arbitrator for up to 64 clients
class RoundRobinArbitrator64
{
private:
	uint64_t pending{0x0ull};
	uint32_t index{0};

public:
	RoundRobinArbitrator64()
	{

	}

	uint num_pending()
	{
		return __popcnt64(pending);
	}

	void add(uint index)
	{
		pending |= 0x1ull << index;
	}

	void add_masked(uint64_t mask)
	{
		pending |= mask;
	}

	void remove(uint index)
	{
		//can't remove what's not there
		assert((pending >> index) & 0x1ull);
		pending &= ~(0x1ull << index);
	}

	void remove_masked(uint64_t mask)
	{
		pending &= ~mask;
	}

	void advance()
	{
		if(!num_pending()) return;

		uint64_t rot_mask = _rotr64(pending, index); //rotate the mask so the last index flag is in the 0 bit
		uint64_t offset = _tzcnt_u64(rot_mask); //count the number of 0s till the next 1
		index = (index + offset) & 0x3f; //advance the index to the next set bit
	}

	uint current()
	{
		if(!(pending & (0x1ull << index))) return ~0;
		return index;
	}
};

class ArbitrationNetwork
{
private:
	std::vector<RoundRobinArbitrator64> arbitrators;

public:
	ArbitrationNetwork()
	{

	}

	ArbitrationNetwork(uint num_in, uint num_out)
	{
		resize(num_in, num_out);
	}

	void resize(uint num_in, uint num_out)
	{
		assert(num_in <= 64);
		arbitrators.resize(num_out);
	}

	void add(uint in, uint out)
	{
		arbitrators[out].add(in);
	}

	void remove(uint in, uint out)
	{
		arbitrators[out].remove(in);
	}

	uint inputs_pending(uint out)
	{
		return arbitrators[out].num_pending();
	}

	uint input(uint out)
	{
		return arbitrators[out].current();
	}

	void update()
	{
		for(uint i = 0; i < arbitrators.size(); ++i)
			arbitrators[i].advance();
	}
};
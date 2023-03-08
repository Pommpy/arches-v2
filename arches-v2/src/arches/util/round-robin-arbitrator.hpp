#pragma once 
#include "../../stdafx.hpp"


//todo this is not a good implmnetation
class RoundRobinArbitrator
{
private:
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
};
#pragma once 
#include "../../stdafx.hpp"


//todo this is not a good implmnetation
class RoundRobinArbitrator
{
private:
	std::vector<uint8_t> request_mask;
	uint size{0};
	uint current_index{0};

public:
	RoundRobinArbitrator(uint size)
	{
		this->size = size;
		request_mask.resize((size + 7) / 8);
	}

	void push_request(uint index)
	{
		uint mask_index = index >> 3;
		uint mask = 0x1ull << (index & 0x7ull);
		request_mask[mask_index] |= mask;
	}

	uint pop_request()
	{
		uint index = current_index;
		do
		{
			uint mask_index = index >> 3;
			uint mask = 0x1ull << (index & 0x7ull);
			if(request_mask[mask_index] & mask)
			{
				request_mask[mask_index] &= ~mask;
				current_index = (index + 1) % size;
				return index;
			}
			index = (index + 1) % size;
		} 
		while(index != current_index);

		return ~0u;
	}
};
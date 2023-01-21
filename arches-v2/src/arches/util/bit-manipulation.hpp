#pragma once
#include "../../stdafx.hpp"

inline uint log2i(uint in)
{
	uint i = 0;
	while (in >>= 1) ++i;
	return i;

	//return __popcnt(in - 1);
}

inline uint64_t generate_nbit_mask(uint n)
{
	return ~(~0ull << n);
}

inline Arches::paddr_t align_to_nbits(Arches::paddr_t paddr, uint n)
{
	uint bytes = log2i(n);
	Arches::paddr_t mask = generate_nbit_mask(bytes);
	return (paddr + mask) & ~mask;
}


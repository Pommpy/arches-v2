#pragma once
#include "../../stdafx.hpp"

inline uint log2i(uint64_t in)
{
	uint i = 0;
	while (in >>= 1) ++i;
	return i;
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

inline uint64_t rotr(uint64_t mask, uint n)
{
	return _rotr64(mask, n);
}

inline uint64_t ctz(uint64_t mask)
{
	return _tzcnt_u64(mask);
}

inline uint64_t popcnt(uint64_t mask)
{
	return __popcnt64(mask);
}
#include "../../stdafx.hpp"

inline uint log2i(uint in)
{
	uint i = 0;
	while (in >>= 1) ++i;
	return i;
}

inline uint64_t generate_nbit_mask(uint n)
{
	return ~(~0ull << n);
}

inline Arches::physical_address align_to_nbits(Arches::physical_address paddr, uint n)
{
	Arches::physical_address bytes = log2i(n);
	Arches::physical_address mask = generate_nbit_mask(bytes);
	return (paddr + mask) & ~mask;
}


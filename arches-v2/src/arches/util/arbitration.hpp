#pragma once 
#include "../../stdafx.hpp"
#include "bit-manipulation.hpp"

//uses a 64bit integer and BM2 extension to implement a computational and stoarge efficent arbitrator for up to 64 clients
class RoundRobinArbitrator64
{
protected:
	uint64_t pending{0x0ull};
	uint     index{0};

public:
	RoundRobinArbitrator64() {}

	uint num_pending()
	{
		return popcnt(pending);
	}

	uint64_t pending_mask()
	{
		return pending;
	}

	void add(uint index)
	{
		add_mask(0x1ull << index);
	}

	void add_mask(uint64_t mask)
	{
		pending |= mask;
	}

	void remove(uint index)
	{
		remove_mask(0x1ull << index);
	}

	void remove_mask(uint64_t mask)
	{
		assert((mask & pending) == mask); //can't remove what's not there
		pending &= ~mask;
	}

	void update()
	{
		uint64_t rot_mask = rotr(pending, index); //rotate the mask so the last index flag is in the 0 bit
		uint64_t offset = ctz(rot_mask); //count the number of 0s till the next 1
		index = (index + offset) & 0x3fu; //advance the index to the next set bit
	}

	uint current()
	{
		if(!((pending >> index) & 0x1ull)) return ~0u;
		return index;
	}
};

#if 1
class ArbitrationNetwork64
{
private:
	std::vector<RoundRobinArbitrator64> arbitrators;

public:
	ArbitrationNetwork64() {}

	ArbitrationNetwork64(uint num_src, uint num_dst)
	{
		resize(num_src, num_dst);
	}

	void resize(uint num_src, uint num_dst)
	{
		assert(num_src <= 64);
		arbitrators.resize(num_dst);
	}

	void add(uint src, uint dst)
	{
		arbitrators[dst].add(src);
	}

	void remove(uint in, uint out)
	{
		arbitrators[out].remove(in);
	}

	uint src(uint dst)
	{
		return arbitrators[dst].current();
	}

	uint srcs_pending(uint dst)
	{
		return arbitrators[dst].num_pending();
	}

	void update()
	{
		for(uint i = 0; i < arbitrators.size(); ++i) arbitrators[i].update();
	}

	void update(uint dst)
	{
		arbitrators[dst].update();
	}
};
#endif
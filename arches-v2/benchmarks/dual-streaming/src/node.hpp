#pragma once
#include "stdafx.hpp"

#include "aabb.hpp"

struct Node
{
	AABB aabb;

	union
	{
		uint32_t data;
		struct
		{
			uint32_t is_leaf : 1;
			uint32_t lst_chld_ofst : 3;
			uint32_t fst_chld_ind : 28;
		};
	};

	uint8_t _pad[4];
};

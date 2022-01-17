#pragma once
#include "stdafx.hpp"

#include "aabb.hpp"

struct Node
{
	AABB aabb;

	uint32_t is_leaf : 1;
	uint32_t lst_chld_ofst : 3;
	uint32_t fst_chld_ind : 28;

	uint32_t _pad;
};

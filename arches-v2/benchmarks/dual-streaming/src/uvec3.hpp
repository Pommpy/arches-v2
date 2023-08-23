#pragma once
#include "stdafx.hpp"

namespace rtm {

class uvec3
{
public:
	uint32_t e[3]{0, 0, 0};

	uvec3() {}
	uvec3(uint32_t i) { e[0] = i;  e[1] = i; e[2] = i; }
	uvec3(uint32_t i, uint32_t j, uint32_t k) { e[0] = i; e[1] = j;  e[2] = k; }

	uint32_t operator[](int i) const { return e[i]; }
	uint32_t& operator[](int i) { return e[i]; }
};

}
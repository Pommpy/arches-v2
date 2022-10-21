#pragma once
#include "stdafx.hpp"

#include"rtm.hpp"

struct Hit
{
	float t {T_MAX};
	rtm::vec2 bc;
	uint prim_id;
};

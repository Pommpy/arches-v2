#pragma once
#include "stdafx.hpp"

#include "rtm.hpp"

struct Ray
{
	rtm::vec3 o;
	float     t_min{T_MIN};
	rtm::vec3 d;
	float     t_max{T_MAX};
};

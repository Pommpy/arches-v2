#pragma once
#include "stdafx.hpp"

#include "rtm.hpp"

struct Ray
{
	rtm::vec3 o;
	float     t_min;
	rtm::vec3 d;
	float     t_max;
	
	float     drdt;
	float     radius;
};

struct Hit
{
	float t;
	rtm::vec2 bc;
	uint32_t id;
};

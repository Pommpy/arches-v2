#pragma once
#include "stdafx.hpp"

#include "rtm.hpp"

struct CompactRay;

struct Ray
{
	rtm::vec3 o;
	rtm::vec3 d;
	rtm::vec3 inv_d;
};

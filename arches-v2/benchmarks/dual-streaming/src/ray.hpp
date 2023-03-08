#pragma once
#include "stdafx.hpp"

#include "rtm.hpp"

#define RAYS_PER_BUCKET 56

struct Ray
{
	rtm::vec3 o;
	rtm::vec3 d;
};

struct BucketRay
{
	Ray  ray;
	uint id;
};

struct Hit
{
	float     t {T_MAX};
	rtm::vec2 bc;
	uint      prim_id;
};
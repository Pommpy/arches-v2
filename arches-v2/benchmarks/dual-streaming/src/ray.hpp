#pragma once
#include "stdafx.hpp"

#include "rtm.hpp"

#define RAYS_PER_BUCKET 56

struct Ray
{
	rtm::vec3 o;
	float     t_min{T_MIN};
	rtm::vec3 d;
	float     t_max{T_MAX};
};

struct BucketRay
{
	uint id;
	Ray  ray;
};

struct RayBucket
{
	RayBucket*    next_bucket;
	volatile uint treelet_id;
	volatile uint num_rays;
	BucketRay     rays[RAYS_PER_BUCKET];
};

struct Hit
{
	float     t {T_MAX};
	rtm::vec2 bc;
	uint      prim_id;
};
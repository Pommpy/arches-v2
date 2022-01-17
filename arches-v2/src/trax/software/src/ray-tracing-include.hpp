#pragma once
#include "stdafx.hpp"

#include "rtm.hpp"
#include "random-number-generator.hpp"
#include "sampling.hpp"
#include "bvh.hpp"
#include "camera.hpp"
#include "ray.hpp"
#include "hit.hpp"
#include "node.hpp"
#include "intersect.hpp"
#include "path.hpp"

#define GLOBAL_DATA_ADDRESS 256ull

struct GlobalData
{
	uint32_t framebuffer_width;
	uint32_t framebuffer_height;
	uint32_t framebuffer_size;
	uint32_t* framebuffer;

	uint32_t samples_per_pixel;
	float inverse_samples_per_pixel;
	uint32_t max_path_depth;

	Camera camera;
	rtm::vec3 light_dir;

	Triangle* triangles;
	Node*     nodes;
};

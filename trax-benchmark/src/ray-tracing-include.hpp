#pragma once
#include "stdafx.hpp"

#include "sampling.hpp"
#include "bvh.hpp"
#include "camera.hpp"
#include "ray.hpp"
#include "intersect.hpp"
#include "mesh.hpp"

#define KERNEL_ARGS_ADDRESS 256ull

struct KernelArgs
{
	uint32_t framebuffer_width;
	uint32_t framebuffer_height;
	uint32_t framebuffer_size;
	uint32_t* framebuffer;

	uint32_t samples_per_pixel;
	uint32_t max_depth;

	rtm::vec3 light_dir;
	MeshPointers mesh;
	Camera camera;
};

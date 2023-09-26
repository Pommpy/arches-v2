#pragma once
#include "stdafx.hpp"

#include "rtm.hpp"
#include "random-number-generator.hpp"
#include "sampling.hpp"
#include "bvh.hpp"
#include "camera.hpp"
#include "intersect.hpp"

#define GLOBAL_DATA_ADDRESS 256ull
#define ATOMIC_REG_FILE_ADDRESS 128ull

struct RayWriteBuffer
{
	uint treelet;
	uint ray_id;
	Ray ray;
};

struct AtomicRegFile
{
	std::atomic_uint32_t regs[32];
};

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

	//heap data pointers
	Triangle* triangles;
	Treelet* treelets;

	//mem mapped address used to issue mem instr
	Hit* hit_records;
	Treelet* scene_buffer;
	void* ray_staging_buffer;
};

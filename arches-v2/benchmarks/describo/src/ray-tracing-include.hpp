#pragma once
#include "stdafx.hpp"

#include "rtm.hpp"
#include "random-number-generator.hpp"
#include "sampling.hpp"
#include "bvh.hpp"
#include "camera.hpp"
#include "ray.hpp"
#include "intersect.hpp"
#include "intersect-tt1.hpp"
#include "intersect-tt2.hpp"
#include "intersect-tt3.hpp"

#define GLOBAL_DATA_ADDRESS 64ull

struct GlobalData
{
	uint32_t framebuffer_width;//68
	uint32_t framebuffer_height;//72
	uint32_t framebuffer_size;//76
	uint32_t* framebuffer;//80

	Camera camera;

	glm::uvec3* ni;
	glm::vec3* normals;

	union
	{
		MeshPointers mesh;
		TesselationTree1Pointers tt1;
		TesselationTree2Pointers tt2;
		TesselationTree3Pointers tt3;
	};
};

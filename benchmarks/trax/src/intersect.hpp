#pragma once
#include "stdafx.hpp"

#include "ray.hpp"
#include "aabb.hpp"
#include "triangle.hpp"
#include "bvh.hpp"

#ifdef ARCH_RISCV
//#define HARDWARE_INTERSECT
#endif

struct MeshPointers
{
	BVH::Node* blas;
	rtm::uvec3* vertex_indices;
	rtm::vec3* vertices;
};

float inline static _rcp(float in)
{
	#ifdef ARCH_RISCV
	float out;
	asm volatile("frcp.s %0,%1\n\t" 
		: "=f" (out) 
		: "f" (in));
	return out;
	#else
	return 1.0f / in;
	#endif
}

inline float intersect(const AABB aabb, const Ray& ray, const rtm::vec3& inv_d)
{
#ifdef HARDWARE_INTERSECT
	register float f3 asm("f3") = ray.o.x;
	register float f4 asm("f4") = ray.o.y;
	register float f5 asm("f5") = ray.o.z;
	register float f6 asm("f6") = ray.t_min;
	register float f7 asm("f7") = inv_d.x;
	register float f8 asm("f8") = inv_d.y;
	register float f9 asm("f9") = inv_d.z;
	register float f10 asm("f10") = ray.t_max;
	register float f11 asm("f11") = aabb.min.x;
	register float f12 asm("f12") = aabb.min.y;
	register float f13 asm("f13") = aabb.min.z;
	register float f14 asm("f14") = aabb.max.x;
	register float f15 asm("f15") = aabb.max.y;
	register float f16 asm("f16") = aabb.max.z;

	float t;
	asm volatile
	(
		"boxisect %0"
		: "=f" (t)
		: "f" (f3), "f" (f4), "f" (f5), "f" (f6), "f" (f7), "f" (f8), "f" (f9), "f" (f10),  "f" (f11), "f" (f12), "f" (f13), "f" (f14),  "f" (f15), "f" (f16)
	);

	return t;
#else
	rtm::vec3 t0 = (aabb.min - ray.o) * inv_d;
	rtm::vec3 t1 = (aabb.max - ray.o) * inv_d;

	rtm::vec3 tminv = rtm::min(t0, t1);
	rtm::vec3 tmaxv = rtm::max(t0, t1);

	float tmin = std::max(std::max(tminv.x, tminv.y), std::max(tminv.z, ray.t_min));
	float tmax = std::min(std::min(tmaxv.x, tmaxv.y), std::min(tmaxv.z, ray.t_max));

	if (tmin > tmax) return ray.t_max;//no hit || behind
	return tmin;
	#endif
}

inline bool intersect(const Triangle tri, const Ray& ray, Hit& hit)
{
#ifdef HARDWARE_INTERSECT
	register float f0 asm("f0") = hit.t;
	register float f1 asm("f1") = hit.bc.x;
	register float f2 asm("f2") = hit.bc.y;
	register float f3 asm("f3") = ray.o.x;
	register float f4 asm("f4") = ray.o.y;
	register float f5 asm("f5") = ray.o.z;
	register float f6 asm("f6") = ray.t_min;
	register float f7 asm("f7") = ray.d.x;
	register float f8 asm("f8") = ray.d.y;
	register float f9 asm("f9") = ray.d.z;
	register float f10 asm("f10") = ray.t_max;
	register float f11 asm("f11") = tri.vrts[0].x;
	register float f12 asm("f12") = tri.vrts[0].y;
	register float f13 asm("f13") = tri.vrts[0].z;
	register float f14 asm("f14") = tri.vrts[1].x;
	register float f15 asm("f15") = tri.vrts[1].y;
	register float f16 asm("f16") = tri.vrts[1].z;
	register float f17 asm("f17") = tri.vrts[2].x;
	register float f18 asm("f18") = tri.vrts[2].y;
	register float f19 asm("f19") = tri.vrts[2].z;

	uint32_t is_hit;
	asm volatile
	(
		"triisect %0"
		: "=r" (is_hit), "+f" (f0), "+f" (f1), "+f" (f2)
		: "f" (f3), "f" (f4), "f" (f5), "f" (f6), "f" (f7), "f" (f8), "f" (f9), "f" (f10),  "f" (f11), "f" (f12), "f" (f13), "f" (f14),  "f" (f15), "f" (f16), "f" (f17), "f" (f18), "f" (f19)
	);

	hit.t = f0;
	hit.bc.x = f1;
	hit.bc.y = f2;

	return is_hit;
#else
	rtm::vec3 bc;
	bc[0] = rtm::dot(rtm::cross(tri.vrts[2] - tri.vrts[1], tri.vrts[1] - ray.o), ray.d);
	bc[1] = rtm::dot(rtm::cross(tri.vrts[0] - tri.vrts[2], tri.vrts[2] - ray.o), ray.d);
	bc[2] = rtm::dot(rtm::cross(tri.vrts[1] - tri.vrts[0], tri.vrts[0] - ray.o), ray.d);
		
	rtm::vec3 gn = rtm::cross(tri.vrts[1] - tri.vrts[0], tri.vrts[2] - tri.vrts[0]);
	float gn_dot_d = rtm::dot(gn, ray.d);

	if(gn_dot_d > 0.0f) bc = -bc;
	if(bc[0] < 0.0f || bc[1] < 0.0f || bc[2] < 0.0f) return false;

	float t = rtm::dot(gn, tri.vrts[0] - ray.o) / gn_dot_d;
	if(t < ray.t_min || t > hit.t) return false;

	hit.bc = rtm::vec2(bc.x, bc.y) / (bc[0] + bc[1] + bc[2]);
	hit.t = t ;
	return true;
#endif
}
inline bool intersect(const MeshPointers& mesh, const Ray& ray, Hit& hit)
{
	rtm::vec3 inv_d = rtm::vec3(1.0f) / ray.d;

	struct NodeStackEntry
	{
		union
		{
			struct
			{
				float t;
				BVH::NodeData data;
			};
			uint64_t _u64;
		};
	};

	NodeStackEntry node_stack[96];
	uint32_t node_stack_size = 1u;
	node_stack[0].t = intersect(mesh.blas[0].aabb, ray, inv_d);
	node_stack[0].data = mesh.blas[0].data;
	
	bool found_hit = false;
	do
	{
		NodeStackEntry current_entry = node_stack[--node_stack_size];
		if(current_entry.t >= hit.t) return found_hit;

		if(!current_entry.data.is_leaf)
		{
			for(uint32_t i = 0; i <= current_entry.data.lst_chld_ofst; ++i)
			{
				uint node_index = current_entry.data.fst_chld_ind + i;
				float t = intersect(mesh.blas[node_index].aabb, ray, inv_d);
				if(t < hit.t)
				{
					uint32_t j = node_stack_size++;
					for(; j != 0; --j)
					{
						if(node_stack[j - 1].t >= t) break;
						node_stack[j]._u64 = node_stack[j - 1]._u64;
					}

					node_stack[j].t = t;
					node_stack[j].data = mesh.blas[node_index].data;
				}
			}
		}
		else
		{
			for(uint32_t i = 0; i <= current_entry.data.lst_chld_ofst; ++i)
			{
				uint32_t id = current_entry.data.fst_chld_ind + i;
				rtm::uvec3 vi = mesh.vertex_indices[id];
				if(intersect(Triangle(mesh.vertices[vi[0]], mesh.vertices[vi[1]], mesh.vertices[vi[2]]), ray, hit))
				{
					hit.id = id;
					found_hit = true;
				}
			}
		}
	} while(node_stack_size);

	return found_hit;
}
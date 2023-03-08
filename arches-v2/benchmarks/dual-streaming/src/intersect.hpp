#pragma once
#include "stdafx.hpp"

#include "ray.hpp"
#include "treelet-bvh.hpp"

inline float intersect(const AABB& aabb, const Ray& ray, const rtm::vec3& inv_d)
{
	#ifdef ARCH_RISCV
	register float f0 asm("f0") = ray.o.x;
	register float f1 asm("f1") = ray.o.y;
	register float f2 asm("f2") = ray.o.z;
	register float f3 asm("f3") = inv_d.x;
	register float f4 asm("f4") = inv_d.y;
	register float f5 asm("f5") = inv_d.z;

	register float f6 asm("f6") = aabb.min.x;
	register float f7 asm("f7") = aabb.min.y;
	register float f8 asm("f8") = aabb.min.z;
	register float f9 asm("f9") = aabb.max.x;
	register float f10 asm("f10") = aabb.max.y;
	register float f11 asm("f11") = aabb.max.z;

	//todo need to block for loads
	float t;
	asm volatile
	(
		"boxisect %0"
		: 
		"=f" (t)
		: 
		"f" (f0),
		"f" (f1),
		"f" (f2),
		"f" (f3),
		"f" (f4),
		"f" (f5),
		"f" (f6),
		"f" (f7),
		"f" (f8),
		"f" (f9),
		"f" (f10),
		"f" (f11)
	);

	return t;
	#endif

	#ifdef ARCH_X86
	rtm::vec3 t0 = (aabb.min - ray.o) * inv_d;
	rtm::vec3 t1 = (aabb.max - ray.o) * inv_d;

	rtm::vec3 tminv = rtm::min(t0, t1);
	rtm::vec3 tmaxv = rtm::max(t0, t1);

	float tmin = std::max(std::max(tminv.x, tminv.y), std::max(tminv.z, T_MIN));
	float tmax = std::min(std::min(tmaxv.x, tmaxv.y), std::min(tmaxv.z, T_MAX));

	if (tmin > tmax || tmax < T_MIN) return T_MAX;//no hit || behind
	return tmin;
	#endif
}

inline bool intersect(const Triangle& tri, const Ray& ray, Hit& hit)
{
#ifdef ARCH_RISCV
	register float f0 asm("f0") = ray.o.x;
	register float f1 asm("f1") = ray.o.y;
	register float f2 asm("f2") = ray.o.z;
	register float f3 asm("f3") = ray.d.x;
	register float f4 asm("f4") = ray.d.y;
	register float f5 asm("f5") = ray.d.z;

	register float f6 asm("f6") = tri.vrts[0].x;
	register float f7 asm("f7") = tri.vrts[0].y;
	register float f8 asm("f8") = tri.vrts[0].z;
	register float f9 asm("f9") = tri.vrts[1].x;
	register float f10 asm("f10") = tri.vrts[1].y;
	register float f11 asm("f11") = tri.vrts[1].z;
	register float f12 asm("f12") = tri.vrts[2].x;
	register float f13 asm("f13") = tri.vrts[2].y;
	register float f14 asm("f14") = tri.vrts[2].z;

	register float f15 asm("f15") = hit.t;
	register float f16 asm("f16") = hit.bc.x;
	register float f17 asm("f17") = hit.bc.y;

	//todo need to block for loads
	uint is_hit;
	asm volatile
	(
		"triisect %0"
		: 
		"=r" (is_hit),
		"+f" (f15),
		"+f" (f16),
		"+f" (f17)
		: 
		"f" (f0),
		"f" (f1),
		"f" (f2),
		"f" (f3),
		"f" (f4),
		"f" (f5),
		"f" (f6),
		"f" (f7),
		"f" (f8),
		"f" (f9),
		"f" (f10),
		"f" (f11),
		"f" (f12),
		"f" (f13),
		"f" (f14)
	);

	hit.t = f15;
	hit.bc.x = f16;
	hit.bc.y = f17;

	return is_hit;
#endif

#ifdef ARCH_X86
	rtm::vec3 bc;
	bc[0] = rtm::dot(rtm::cross(tri.vrts[2] - tri.vrts[1], tri.vrts[1] - ray.o), ray.d);
	bc[1] = rtm::dot(rtm::cross(tri.vrts[0] - tri.vrts[2], tri.vrts[2] - ray.o), ray.d);
	bc[2] = rtm::dot(rtm::cross(tri.vrts[1] - tri.vrts[0], tri.vrts[0] - ray.o), ray.d);
		
	if(bc[0] < 0.0f || bc[1] < 0.0f || bc[2] < 0.0f) return false;

	rtm::vec3 gn = rtm::cross(tri.vrts[1] - tri.vrts[0], tri.vrts[2] - tri.vrts[0]);
	float gn_dot_d = rtm::dot(gn, ray.d);

	//TODO divides should be done in software
	float t = rtm::dot(gn, tri.vrts[0] - ray.o) / gn_dot_d;

	if(t < T_MIN || t > hit.t) return false;

	hit.bc = rtm::vec2(bc.x, bc.y) / (bc[0] + bc[1] + bc[2]);
	hit.t = t;
	return true;
#endif
}

bool inline intersect_treelet(const Treelet& treelet, const Ray& ray, Hit& hit, uint* treelet_stack, uint& treelet_stack_size)
{
	rtm::vec3 inv_d = rtm::vec3(1.0f) / ray.d;

	struct NodeStackEntry
	{
		float hit_t{T_MAX};
		union
		{
			uint32_t data;
			struct
			{
				uint32_t is_leaf : 1;
				uint32_t is_treelet_leaf : 1;
				uint32_t last_tri_offset : 3;
				uint32_t child_index : 27;
			};
		};
	};

	uint node_stack_index = 0u; NodeStackEntry node_stack[32];
	node_stack[0].hit_t = T_MIN;
	node_stack[0].is_leaf = 0;
	node_stack[0].is_treelet_leaf = 0;
	node_stack[0].child_index = 0;

	bool is_hit = false;
	while(node_stack_index != ~0u)
	{
		NodeStackEntry current_entry = node_stack[node_stack_index--];
		if(current_entry.hit_t >= hit.t) continue;

	TRAV:
		if(!current_entry.is_leaf)
		{
			if(current_entry.is_treelet_leaf)
			{
				treelet_stack[treelet_stack_size++] = current_entry.child_index;
			}
			else
			{
				TreeletNode nodes_local[2];
				nodes_local[0] = ((TreeletNode*)&treelet._words[current_entry.child_index])[0];
				nodes_local[1] = ((TreeletNode*)&treelet._words[current_entry.child_index])[1];

				float hit_ts[2];
				hit_ts[0] = intersect(nodes_local[0].aabb, ray, inv_d);
				hit_ts[1] = intersect(nodes_local[1].aabb, ray, inv_d);

				if(hit_ts[0] < hit_ts[1])
				{

					if(hit_ts[1] < hit.t) node_stack[++node_stack_index] = {hit_ts[1], nodes_local[1].data};
					if(hit_ts[0] < hit.t)
					{
						current_entry = {hit_ts[0], nodes_local[0].data};
						goto TRAV;
					}
				}
				else
				{
					if(hit_ts[0] < hit.t) node_stack[++node_stack_index] = {hit_ts[0], nodes_local[0].data};
					if(hit_ts[1] < hit.t)
					{
						current_entry = {hit_ts[1], nodes_local[1].data};
						goto TRAV;
					}
				}
			}
		}
		else
		{
			TreeletTriangle* tris = (TreeletTriangle*)(&treelet._words[current_entry.child_index]);
			for(uint i = 0; i <= current_entry.last_tri_offset; ++i)
			{
				TreeletTriangle tri = tris[i];
				if(intersect(tri.tri, ray, hit))
				{
					hit.prim_id = tri.id;
					is_hit |= true;
				}
			}
		}
	}

	return is_hit;
}

bool inline intersect(const Treelet* treelets, const Ray& ray, Hit& hit)
{
	uint treelet_stack_size = 1u;  uint treelet_stack[64];
	treelet_stack[0] = 0;

	bool is_hit = false;
	while(treelet_stack_size != 0u)
	{
		//TODO in dual streaming this comes from the current scene segment we are traversing
		uint treelet_index = treelet_stack[--treelet_stack_size];
		is_hit |= intersect_treelet(treelets[treelet_index], ray, hit, treelet_stack, treelet_stack_size);
	}

	return is_hit;
}

BucketRay _lbray(const void* ray_staging_buffer, uint& treelet_index)
{
#ifdef ARCH_RISCV
	register float f0 asm("f0");
	register float f1 asm("f1");
	register float f2 asm("f2");
	register float f3 asm("f3");
	register float f4 asm("f4");
	register float f5 asm("f5");
	register float f6 asm("f6");
	register float f7 asm("f7");

	//todo need to block for loads
	float t;
	asm volatile
	(
		"lbray %0, 0(%8)"
		:
		"=f" (f0),
		"=f" (f1),
		"=f" (f2),
		"=f" (f3),
		"=f" (f4),
		"=f" (f5),
		"=f" (f6),
		"=f" (f7)
		:
		"r" (ray_staging_buffer)
	);

	BucketRay bray;
	bray.ray.o.x = f0;
	bray.ray.o.y = f1;
	bray.ray.o.z = f2;
	bray.ray.d.x = f3;
	bray.ray.d.y = f4;
	bray.ray.d.z = f5;

	float _f8 = f6;
	float _f9 = f7;

	bray.id = *(uint*)&_f8;
	treelet_index = *(uint*)&_f9;

	return bray;
#else
	return BucketRay();
#endif
}

void _sbray(const BucketRay& bray, const uint treelet_index, const void* ray_staging_buffer)
{
#ifdef ARCH_RISCV
	register float f0 asm("f0") = bray.ray.o.x;
	register float f1 asm("f1") = bray.ray.o.y;
	register float f2 asm("f2") = bray.ray.o.z;
	register float f3 asm("f3") = bray.ray.d.x;
	register float f4 asm("f4") = bray.ray.d.y;
	register float f5 asm("f5") = bray.ray.d.z;
	register float f6 asm("f6") = *(float*)&bray.id;
	register float f7 asm("f7") = *(float*)&treelet_index;

	//todo need to block for loads
	float t;
	asm volatile
	(
		"sbray %1, 0(%0)"
		:
		:
		"r" (ray_staging_buffer),
		"f" (f0),
		"f" (f1),
		"f" (f2),
		"f" (f3),
		"f" (f4),
		"f" (f5),
		"f" (f6),
		"f" (f7)
	);
#endif
}

void _cshit(const Hit& src, Hit* dst)
{
#ifdef ARCH_RISCV
	register float f17 asm("f17") = src.t;
	register float f18 asm("f18") = src.bc.x;
	register float f19 asm("f19") = src.bc.y;
	register float f20 asm("f20") = *(float*)&src.prim_id;

	//todo need to block for loads
	float t;
	asm volatile
	(
		"sbray %1, 0(%0)"
		:
		:
		"r" (dst),
		"f" (f17),
		"f" (f18),
		"f" (f19),
		"f" (f20)
	);
#endif
}

bool inline intersect_buckets(void* ray_staging_buffer, const Treelet* scene_buffer, Hit* hit_records)
{
	uint treelet_stack_size = 0u;  uint treelet_stack[64];

	bool is_hit = false;
	while(1)
	{
		uint treelet_index;
		BucketRay bray = _lbray(ray_staging_buffer, treelet_index);

		Hit hit; hit.t = T_MAX; //TODO should we load hit t from hit records
		//TODO in dual streaming this comes from the current scene segment we are traversing
		if(intersect_treelet(scene_buffer[treelet_index], bray.ray, hit, treelet_stack, treelet_stack_size))
		{
			//update hit record with hit using cshit
			_cshit(hit, hit_records + bray.id);
		}
		
		//drain treelet stack
		for(uint i = 0; i < treelet_stack_size; ++i)
			_sbray(bray, treelet_stack[i], ray_staging_buffer);

		treelet_stack_size = 0;
	}

	return is_hit;
}


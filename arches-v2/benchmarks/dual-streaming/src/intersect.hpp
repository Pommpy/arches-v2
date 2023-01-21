#pragma once
#include "stdafx.hpp"

#include "ray.hpp"
#include "treelet-bvh.hpp"

inline float intersect(const AABB& aabb, const Ray& ray, const rtm::vec3& inv_d)
{
	#ifdef ARCH_RISCV
	register float ray_o_x   asm("f3") = ray.o.x;
	register float ray_o_y   asm("f4") = ray.o.y;
	register float ray_o_z   asm("f5") = ray.o.z;
	register float ray_t_min asm("f6") = ray.t_min;
	register float inv_d_x   asm("f7") = inv_d.x;
	register float inv_d_y   asm("f8") = inv_d.y;
	register float inv_d_z   asm("f9") = inv_d.z;
	register float ray_t_max asm("f10") = ray.t_max;

	register float min_x asm("f11") = aabb.min.x;
	register float min_y asm("f12") = aabb.min.y;
	register float min_z asm("f13") = aabb.min.z;
	register float max_x asm("f14") = aabb.max.x;
	register float max_y asm("f15") = aabb.max.y;
	register float max_z asm("f16") = aabb.max.z;

	//todo need to block for loads
	float t;
	asm volatile
	(
		"boxisect %[_t]"
		: 
		[_t] "=f" (t)
		: 
		[_ray_o_x]   "f" (ray_o_x),
		[_ray_o_y]   "f" (ray_o_y),
		[_ray_o_z]   "f" (ray_o_z),
		[_ray_t_min] "f" (ray_t_min),
		[_inv_d_x]   "f" (inv_d_x),
		[_inv_d_y]   "f" (inv_d_y),
		[_inv_d_z]   "f" (inv_d_z),
		[_ray_t_max] "f" (ray_t_max),

		[_min_x] "f" (min_x),
		[_min_y] "f" (min_y),
		[_min_z] "f" (min_z),
		[_max_x] "f" (max_x),
		[_max_y] "f" (max_y),
		[_max_z] "f" (max_z)
	);

	return t;
	#endif

	#ifdef ARCH_X86
	rtm::vec3 t0 = (aabb.min - ray.o) * inv_d;
	rtm::vec3 t1 = (aabb.max - ray.o) * inv_d;

	rtm::vec3 tminv = rtm::min(t0, t1);
	rtm::vec3 tmaxv = rtm::max(t0, t1);

	float tmin = std::max(std::max(tminv.x, tminv.y), std::max(tminv.z, ray.t_min));
	float tmax = std::min(std::min(tmaxv.x, tmaxv.y), std::min(tmaxv.z, ray.t_max));

	if (tmin > tmax || tmax < ray.t_min) return ray.t_max;//no hit || behind
	return tmin;
	#endif
}

inline bool intersect(const Triangle& tri, const Ray& ray, Hit& hit)
{
#ifdef ARCH_RISCV
	register float hit_t       asm("f0") = hit.t;
	register float hit_bc_x    asm("f1") = hit.bc.x;
	register float hit_bc_y    asm("f2") = hit.bc.y;

	register float ray_o_x   asm("f3") = ray.o.x;
	register float ray_o_y   asm("f4") = ray.o.y;
	register float ray_o_z   asm("f5") = ray.o.z;
	register float ray_t_min asm("f6") = ray.t_min;
	register float ray_d_x   asm("f7") = ray.d.x;
	register float ray_d_y   asm("f8") = ray.d.y;
	register float ray_d_z   asm("f9") = ray.d.z;
	register float ray_t_max asm("f10") = ray.t_max;

	register float vrts_0_x asm("f11") = tri.vrts[0].x;
	register float vrts_0_y asm("f12") = tri.vrts[0].y;
	register float vrts_0_z asm("f13") = tri.vrts[0].z;
	register float vrts_1_x asm("f14") = tri.vrts[1].x;
	register float vrts_1_y asm("f15") = tri.vrts[1].y;
	register float vrts_1_z asm("f16") = tri.vrts[1].z;
	register float vrts_2_x asm("f17") = tri.vrts[2].x;
	register float vrts_2_y asm("f18") = tri.vrts[2].y;
	register float vrts_2_z asm("f19") = tri.vrts[2].z;

	//todo need to block for loads
	uint is_hit;
	asm volatile
	(
		"triisect %[_is_hit]"
		: 
		[_is_hit] "=r" (is_hit)
		: 
		[_hit_t]     "f" (hit_t),
		[_hit_bc_x]  "f" (hit_bc_x),
		[_hit_bc_y]  "f" (hit_bc_y),

		[_ray_o_x]   "f" (ray_o_x),
		[_ray_o_y]   "f" (ray_o_y),
		[_ray_o_z]   "f" (ray_o_z),
		[_ray_t_min] "f" (ray_t_min),
		[_ray_d_x]   "f" (ray_d_x),
		[_ray_d_y]   "f" (ray_d_y),
		[_ray_d_z]   "f" (ray_d_z),
		[_ray_t_max] "f" (ray_t_max),

		[_vrts_0_x] "f" (vrts_0_x),
		[_vrts_0_y] "f" (vrts_0_y),
		[_vrts_0_z] "f" (vrts_0_z),
		[_vrts_1_x] "f" (vrts_1_x),
		[_vrts_1_y] "f" (vrts_1_y),
		[_vrts_1_z] "f" (vrts_1_z),
		[_vrts_2_x] "f" (vrts_2_x),
		[_vrts_2_y] "f" (vrts_2_y),
		[_vrts_2_z] "f" (vrts_2_z)
	);

	hit.t = hit_t;
	hit.bc.x = hit_bc_x;
	hit.bc.y = hit_bc_y;

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

	if(t < ray.t_min || t > hit.t) return false;

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
	node_stack[0].hit_t = ray.t_min;
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

bool inline intersect_buckets(const RayBucket& ray_staging_buffer, const Treelet* treelets, const Ray* rays, Hit& hit)
{
	uint treelet_stack_size = 1u;  uint treelet_stack[64];
	treelet_stack[0] = 0;	

	bool is_hit = false;
	while(1)
	{
		uint ray_index = ray_staging_buffer.num_rays;
		if(ray_index = ~0u) break;
		Ray ray = rays[ray_index];
		uint treelet_index = ray_staging_buffer.treelet_id;

		//TODO in dual streaming this comes from the current scene segment we are traversing
		is_hit |= intersect_treelet(treelets[treelet_index], ray, hit, treelet_stack, treelet_stack_size);
	}

	return is_hit;
}


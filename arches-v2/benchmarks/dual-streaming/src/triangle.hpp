#pragma once
#include "stdafx.hpp"

#include "rtm.hpp"
#include "aabb.hpp"
#include "ray.hpp"
#include "hit.hpp"

class Triangle
{
public:
	rtm::vec3 vrts[3]{rtm::vec3(0.0f), rtm::vec3(0.0f), rtm::vec3(0.0f)};

public:
	Triangle() {}
	Triangle(const rtm::vec3& v0, const rtm::vec3& v1, const rtm::vec3& v2)
	{
		this->vrts[0] = v0;
		this->vrts[1] = v1;
		this->vrts[2] = v2;
	}

	AABB get_aabb() const
	{
		AABB aabb;
		for(uint i = 0; i < 3; ++i)
		{
			aabb.min = rtm::min(aabb.min, vrts[i]);
			aabb.max = rtm::max(aabb.max, vrts[i]);
		}
		return aabb;
	}

	float get_cost() const { return 2.0f; }

	rtm::vec3 get_normal()
	{
		return rtm::normalize(rtm::cross(vrts[1] - vrts[0], vrts[2] - vrts[0]));
	}

	#if 1
	bool inline intersect(const Ray& ray, Hit& hit) const
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

		register float vrts_0_x asm("f11") = vrts[0].x;
		register float vrts_0_y asm("f12") = vrts[0].y;
		register float vrts_0_z asm("f13") = vrts[0].z;
		register float vrts_1_x asm("f14") = vrts[1].x;
		register float vrts_1_y asm("f15") = vrts[1].y;
		register float vrts_1_z asm("f16") = vrts[1].z;
		register float vrts_2_x asm("f17") = vrts[2].x;
		register float vrts_2_y asm("f18") = vrts[2].y;
		register float vrts_2_z asm("f19") = vrts[2].z;

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
        rtm::vec3 edge1  = vrts[0] - vrts[2];
        rtm::vec3 edge2  = vrts[1] - vrts[2];
       	//rtm::vec3 gn = rtm::cross(edge1, edge2);

        rtm::vec3 r1 = rtm::cross(ray.d, edge2);
        float denomenator = rtm::dot(edge1, r1);
        float inverse = 1.0f / denomenator;

        rtm::vec3 s = ray.o - vrts[2];
        float b1 = rtm::dot(s, r1) * inverse;
        if (b1 < 0.0f || b1 > 1.0f) return false;

        rtm::vec3 r2 = rtm::cross(s, edge1);
        float b2  = rtm::dot(ray.d, r2) * inverse;
        if (b2 < 0.0f || (b2 + b1) > 1.0f) return false;

        float t = rtm::dot(edge2, r2) * inverse;
       	if(t < ray.t_min || t > hit.t) return false;

		hit.t = t;
		hit.bc = rtm::vec2(b1, b2);
		return true;
		#endif
	}
	#else
	bool inline intersect(const Ray& ray, Hit& hit) const
	{
		rtm::vec3 bc;
		bc[0] = rtm::dot(rtm::cross(vrts[2] - vrts[1], vrts[1] - ray.o), ray.d);
		if(bc[0] < 0.0f) return false;
		bc[1] = rtm::dot(rtm::cross(vrts[0] - vrts[2], vrts[2] - ray.o), ray.d);
		if(bc[1] < 0.0f) return false;
		bc[2] = rtm::dot(rtm::cross(vrts[1] - vrts[0], vrts[0] - ray.o), ray.d);
		if(bc[2] < 0.0f) return false;

		rtm::vec3 gn = rtm::cross(vrts[1] - vrts[0], vrts[2] - vrts[0]);
		float gn_dot_d = rtm::dot(gn, ray.d);
		float t = rtm::dot(gn, vrts[0] - ray.o) / gn_dot_d;
		if(t < ray.t_min || t > hit.t) return false;

		//bc /= (bc[0] + bc[1] + bc[2]);
		hit.t = t;
		hit.normal = rtm::normalize(gn); 
		return true;
	}
	#endif
};

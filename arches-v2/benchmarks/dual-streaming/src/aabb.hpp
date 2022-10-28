#pragma once
#include "stdafx.hpp"

#include "ray.hpp"
#include "rtm.hpp"

class AABB
{
public:
	rtm::vec3 min{ FLT_MAX };
	rtm::vec3 max{ -FLT_MAX };

public:
	AABB() {}
	~AABB() {}

	#if 0 

	float intersect(const Ray& ray, const rtm::vec3& inv_d) const
	{
		float tmin = (min.x - ray.o.x) * inv_d.x;
		float tmax = (max.x - ray.o.x) * inv_d.x;

		if (tmin > tmax) { float temp = tmin; tmin = tmax; tmax = temp; }

		float tymin = (min.y - ray.o.y) * inv_d.y;
		float tymax = (max.y - ray.o.y) * inv_d.y;

		if (tymin > tymax) { float temp = tymin; tymin = tymax; tymax = temp; }

		if ((tmin > tymax) || (tymin > tmax)) return T_MAX;
		if (tymin > tmin) tmin = tymin;
		if (tymax < tmax) tmax = tymax;

		float tzmin = (min.z - ray.o.z) * inv_d.z;
		float tzmax = (max.z - ray.o.z) * inv_d.z;

		if (tzmin > tzmax) { float temp = tzmin; tzmin = tzmax; tzmax = temp; }

		if ((tmin > tzmax) || (tzmin > tmax)) return T_MAX;
		if (tzmin > tmin) tmin = tzmin;
		if (tzmax < tmax) tmax = tzmax;

		if (tmax < ray.t_min) return ray.t_max;

		return tmin;
	}
	#else 
	float intersect(const Ray& ray, const rtm::vec3& inv_d) const
	{
		#ifdef ARCH_RISCV
		register float ray_o_x   asm("f3") = ray.o.x;
		register float ray_o_y   asm("f4") = ray.o.y;
		register float ray_o_z   asm("f5") = ray.o.z;
		register float ray_t_min asm("f6") = ray.t_min;
		register float ray_d_x   asm("f7") = ray.d.x;
		register float ray_d_y   asm("f8") = ray.d.y;
		register float ray_d_z   asm("f9") = ray.d.z;
		register float ray_t_max asm("f10") = ray.t_max;

		register float min_x asm("f11") = min.x;
		register float min_y asm("f12") = min.y;
		register float min_z asm("f13") = min.z;
		register float max_x asm("f14") = max.x;
		register float max_y asm("f15") = max.y;
		register float max_z asm("f16") = max.z;

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
			[_ray_d_x]   "f" (ray_d_x),
			[_ray_d_y]   "f" (ray_d_y),
			[_ray_d_z]   "f" (ray_d_z),
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
		rtm::vec3 t0 = (min - ray.o) * inv_d;
		rtm::vec3 t1 = (max - ray.o) * inv_d;

		rtm::vec3 tminv = rtm::min(t0, t1);
		rtm::vec3 tmaxv = rtm::max(t0, t1);

		float tmin =  std::max(std::max(tminv.x, tminv.y), std::max(tminv.z, ray.t_min));
		float tmax = std::min(std::min(tmaxv.x, tmaxv.y), std::min(tmaxv.z, ray.t_max));

		if (tmin > tmax || tmax < ray.t_min) return ray.t_max;//no hit || behind
		return tmin;
		#endif
	}
	#endif

	void add_aabb(const AABB& other)
	{
		this->min = rtm::min(this->min, other.min);
		this->max = rtm::max(this->max, other.max);
	}

	float surface_area() const
	{
		float x = max.x - min.x;
		float y = max.y - min.y;
		float z = max.z - min.z;

		return (x * y + y * z + z * x) * 2.0f;
	}

	float get_cost() const { return 1.0f; };

	rtm::vec3 get_centroid() const { return (min + max) * 0.5f; }

	uint get_longest_axis() const
	{
		uint axis = 0;
		float max_length = 0.0f;
		for(uint i = 0; i < 3; ++i)
		{
			float length = max[i] - min[i];
			if(length > max_length)
			{
				axis = i;
				max_length = length;
			}
		}
		return axis;
	}
};

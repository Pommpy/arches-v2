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
		rtm::vec3 t0 = (min - ray.o) * inv_d;
		rtm::vec3 t1 = (max - ray.o) * inv_d;

		rtm::vec3 tminv = rtm::min(t0, t1);
		rtm::vec3 tmaxv = rtm::max(t0, t1);

		float tmin =  std::max(std::max(tminv.x, tminv.y), std::max(tminv.z, ray.t_min));
		float tmax = std::min(std::min(tmaxv.x, tmaxv.y), std::min(tmaxv.z, ray.t_max));

		if (tmin > tmax || tmax < ray.t_min) return ray.t_max;//no hit || behind
		return tmin;
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

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

	float intersect(const Ray& ray) const
	{
		float tmin = (min.x - ray.o.x) * ray.inv_d.x;
		float tmax = (max.x - ray.o.x) * ray.inv_d.x;

		if (tmin > tmax) { float temp = tmin; tmin = tmax; tmax = temp; }

		float tymin = (min.y - ray.o.y) * ray.inv_d.y;
		float tymax = (max.y - ray.o.y) * ray.inv_d.y;

		if (tymin > tymax) { float temp = tymin; tymin = tymax; tymax = temp; }

		if ((tmin > tymax) || (tymin > tmax)) return T_MAX;
		if (tymin > tmin) tmin = tymin;
		if (tymax < tmax) tmax = tymax;

		float tzmin = (min.z - ray.o.z) * ray.inv_d.z;
		float tzmax = (max.z - ray.o.z) * ray.inv_d.z;

		if (tzmin > tzmax) { float temp = tzmin; tzmin = tzmax; tzmax = temp; }

		if ((tmin > tzmax) || (tzmin > tmax)) return T_MAX;
		if (tzmin > tmin) tmin = tzmin;
		if (tzmax < tmax) tmax = tzmax;

		if (tmax < T_MIN) return T_MAX;

		return tmin;
	}

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

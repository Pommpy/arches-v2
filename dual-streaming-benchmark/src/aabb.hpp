#pragma once
#include "stdafx.hpp"

#include "rtm.hpp"

class AABB
{
public:
	rtm::vec3 min{ FLT_MAX };
	rtm::vec3 max{ -FLT_MAX };

public:
	AABB() {}

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

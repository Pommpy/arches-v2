#pragma once

#include "aabb.hpp"

namespace rtm
{

class Triangle
{
public:
	rtm::vec3 vrts[3];

public:
	Triangle() = default;

	Triangle(const rtm::vec3& v0, const rtm::vec3& v1, const rtm::vec3& v2)
	{
		this->vrts[0] = v0;
		this->vrts[1] = v1;
		this->vrts[2] = v2;
	}

	AABB aabb() const
	{
		AABB aabb;
		for(int i = 0; i < 3; ++i)
		{
			aabb.min = rtm::min(aabb.min, vrts[i]);
			aabb.max = rtm::max(aabb.max, vrts[i]);
		}
		return aabb;
	}

	static float cost() { return 2.0f; }

	rtm::vec3 normal()
	{
		return rtm::normalize(rtm::cross(vrts[0] - vrts[2], vrts[1] - vrts[2]));
	}
};

}
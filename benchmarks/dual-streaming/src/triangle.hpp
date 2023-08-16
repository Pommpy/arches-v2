#pragma once
#include "stdafx.hpp"

#include "rtm.hpp"
#include "aabb.hpp"

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


};

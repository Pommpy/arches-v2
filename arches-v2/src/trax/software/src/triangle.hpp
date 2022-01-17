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

	bool inline intersect(const Ray& ray, Hit& hit) const
	{
		rtm::vec3 gn = rtm::cross(vrts[1] - vrts[0], vrts[2] - vrts[0]);

		float gn_dot_d = rtm::dot(gn, ray.d);
		float t = rtm::dot(gn, vrts[0] - ray.o) / gn_dot_d;

		if(gn_dot_d < 0.0f && t > T_MIN && t < hit.t)
		{
			rtm::vec3 hit_pos = t * ray.d + ray.o;
			rtm::vec2 vs2d[3], hp2d;
			float abs_gnx = abs(gn.x), abs_gny = abs(gn.y), abs_gnz = abs(gn.z);
			if(abs_gnx > abs_gny && abs_gnx > abs_gnz)
			{
				vs2d[0] = rtm::vec2(vrts[0].y, vrts[0].z);
				vs2d[1] = rtm::vec2(vrts[1].y, vrts[1].z);
				vs2d[2] = rtm::vec2(vrts[2].y, vrts[2].z);
				hp2d = rtm::vec2(hit_pos.y, hit_pos.z);
			}
			else if(abs_gny > abs_gnz)
			{
				vs2d[0] = rtm::vec2(vrts[0].x, vrts[0].z);
				vs2d[1] = rtm::vec2(vrts[1].x, vrts[1].z);
				vs2d[2] = rtm::vec2(vrts[2].x, vrts[2].z);
				hp2d = rtm::vec2(hit_pos.x, hit_pos.z);
			}
			else
			{
				vs2d[0] = rtm::vec2(vrts[0].x, vrts[0].y);
				vs2d[1] = rtm::vec2(vrts[1].x, vrts[1].y);
				vs2d[2] = rtm::vec2(vrts[2].x, vrts[2].y);
				hp2d = rtm::vec2(hit_pos.x, hit_pos.y);
			}

			float a = rtm::cross(vs2d[1] - vs2d[0], vs2d[2] - vs2d[0]);
			rtm::vec3 bary_coords;
			bary_coords[0] = rtm::cross(vs2d[1] - hp2d, vs2d[2] - hp2d) / a;
			bary_coords[1] = rtm::cross(vs2d[2] - hp2d, vs2d[0] - hp2d) / a;
			bary_coords[2] = rtm::cross(vs2d[0] - hp2d, vs2d[1] - hp2d) / a;

			if(bary_coords[0] >= 0.0f && bary_coords[1] >= 0.0f && bary_coords[2] >= 0.0f)
			{
				hit.t = t;
				hit.normal = rtm::normalize(gn);
				return true;
			}
		}
		return false;
	}
};

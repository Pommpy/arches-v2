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

    	//bc /= (bc[0] + bc[1] + bc[2]);
		hit.t = t;
		//hit.normal = rtm::normalize(gn); 
		return true;
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

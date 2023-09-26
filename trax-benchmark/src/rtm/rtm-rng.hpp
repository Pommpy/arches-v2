#pragma once

#include "rtm-float.hpp"
#include "rtm-vec2.hpp"
#include "rtm-vec3.hpp"
#include "rtm-vec4.hpp"

namespace rtm
{

class RNG
{
public:
	static uint32_t hash(uint32_t u)
	{
		return 1664525 * u + 1013904223;
	}

private:
	uint32_t _state{ 0u };

	float _build_float(uint32_t u)
	{
		uint32_t bits = u & 0x007FFFFFu | 0x3F800000u;
		return *((float*)(&bits)) - 1.0f;
	}

public:
	RNG(uint32_t seed = 0)
	{
		_state = hash(seed);
	}

	float randf()
	{
		uint32_t r = _state;
		_state = hash(_state);
		return _build_float(r);
	}

	rtm::vec2 randv2() { return rtm::vec2(randf(), randf()); }
	rtm::vec3 randv3() { return rtm::vec3(randf(), randf(), randf()); }
	rtm::vec4 randv4() { return rtm::vec4(randf(), randf(), randf(), randf()); }

	uint32_t randi()
	{
		uint32_t r = _state;
		_state = hash(_state);
		return r;
	}
};

}
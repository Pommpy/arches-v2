#pragma once
#include "stdafx.hpp"

#include "ray.hpp"

class Camera
{
private:
	uint _height, _width;
	rtm::vec2 _recip_size;
	rtm::vec3 _position;
	rtm::vec3 _x, _y, _z;

public:
	struct CameraConfig
	{
		uint img_width;
		uint img_height;
		float fov{60.0f};
		rtm::vec3 position {0.0f, 0.0f, 0.0f};
		rtm::vec3 look_at {0.0f, 0.0f, 0.0f};
		rtm::vec3 up {0.0f, 1.0f, 0.0f};
	};

	#ifdef ARCH_X86
	Camera(uint width = 1, uint height = 1, float fov = 60.0f,
		//float aperture = 0.0f, float focal_length = 3.0f,
		rtm::vec3 position = rtm::vec3(0.0f, 0.0f, 1.0f),
		rtm::vec3 target = rtm::vec3(0.0f, 0.0f, 0.0f),
		rtm::vec3 up = rtm::vec3(0.0f, 1.0f, 0.0f))
	{
		_position = position;
		rtm::vec3 direction = target - position;
		_z = -rtm::normalize(direction);
		_x = rtm::normalize(cross(up, _z));
		_y = rtm::cross(_z, _x);

		float image_plane_height = std::tan(fov * PI / 180.0f / 2.0f);
		float image_plane_width = image_plane_height * float(width) / float(height);
		_x *= image_plane_width;
		_y *= -image_plane_height;

		//this->focal_length = focal_length;
		//this->aperture = aperture;

		_height = height;
		_width = width;
		_recip_size = rtm::vec2(2.0f) / rtm::vec2(static_cast<float>(width, static_cast<float>(height)));
	}
	#endif

	uint get_width() {return _width;}
	uint get_height() {return _height;}

	void generate_ray_through_pixel(uint i, uint j, Ray& ray, RNG& rng) const
	{
		rtm::vec2 uv = (rtm::vec2(static_cast<float>(i), static_cast<float>(j)) + rtm::vec2(0.5f)) * _recip_size - rtm::vec2(1.0f);
		ray.d = rtm::normalize(_x * uv.x + _y * uv.y - _z);
		ray.o = _position;
	}
};

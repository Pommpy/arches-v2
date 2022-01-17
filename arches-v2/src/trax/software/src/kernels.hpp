#pragma once
#include "stdafx.hpp"

#include "custom-instr.hpp"
#include "ray-tracing-include.hpp"

#ifdef ARCH_X86
//replace arbitrary address with the global data we allocate when running nativly
extern GlobalData global_data;
#define _KERNEL_AGRS ;
#endif

#ifdef ARCH_RISCV
#define _KERNEL_AGRS GlobalData& global_data = *reinterpret_cast<GlobalData*>(GLOBAL_DATA_ADDRESS);
#endif

/*
extern "C" void primary_ray_gen()
{
	_KERNEL_AGRS;
	for(uint index = atomicinc(); index < global_data.framebuffer_size; index = atomicinc())
	{
		uint x = index % global_data.framebuffer_width;
		uint y = index / global_data.framebuffer_width;
		RNG rng(index);

		Ray ray;
		global_data.camera.generate_ray_through_pixel(x, y, ray, rng);
		ray.inv_d = rtm::vec3(1.0f) / ray.d;
		global_data.ray_buffer[index] = ray;
		global_data.radiance_buffer[index] = ray.d * 0.5f + rtm::vec3(0.5f);
	}
}

extern "C" void trace_rays()
{
	_KERNEL_AGRS;
	for(uint index = atomicinc(); index < global_data.framebuffer_size; index = atomicinc())
	{
		Hit hit;
		Ray ray = global_data.ray_buffer[index];
		rtm::vec3 color(0.0f);
		intersect(global_data.nodes, global_data.triangles, ray, false, hit);
		global_data.hit_buffer[index] = hit;
		global_data.radiance_buffer[index] = rtm::vec3(hit.t / 1024.0f);
	}
}
*/

/*
void diffuse_ray_gen()
{
	_KERNEL_AGRS;
	for(uint index = atomicinc(); index < global_data.framebuffer_size; index = atomicinc())
	{
		RNG rng(index);

		Ray ray = global_data.ray_buffer[index];
		Hit hit = global_data.hit_buffer[index];
		ray.o += hit.t * ray.d;
		rtm::vec3 normal = global_data.normal_buffer[index];
		ray.d = cosine_sample_hemisphere(normal, rng);
		ray.inv_d = rtm::vec3(1.0f) / ray.d;
		global_data.ray_buffer[index] = ray;
	}
}

void trace_shadow_rays(uint index, GlobalData& global_data)
{
	_KERNEL_AGRS;
	for(uint index = atomicinc(); index < global_data.framebuffer_size; index = atomicinc())
	{
		Hit hit = global_data.hit_buffer[index];
		Ray ray = global_data.ray_buffer[index];
		rtm::vec3 normal = global_data.normal_buffer[index];

		float shading = rtm::dot(global_data.light_dir, global_data.normal_buffer[index]);
		if(shading > 0.0f)
		{
			Ray shadow_ray; Hit shadow_hit;
			shadow_ray.o = ray.o + ray.d * hit.t;
			shadow_ray.d = global_data.light_dir;
			shadow_ray.inv_d = rtm::vec3(1.0f) / shadow_ray.d;
			bvh.intersect(shadow_ray, shadow_hit, true);
			if(shadow_hit.t >= T_MAX)
				global_data.radiance_buffer[index] += global_data.throuput_buffer[index] * rtm::vec3(0.8f, 0.75f, 0.7f) * shading;
		}

		global_data.ray_buffer[index].o = ray.o;
	}
}

int shade_hit()
{
	_KERNEL_AGRS;
	for(uint index = atomicinc(); index < global_data.framebuffer_size; index = atomicinc())
	{
		Hit hit = global_data.hit_buffer[index];
		Ray ray = global_data.ray_buffer[index];
		rtm::vec3 normal = mesh.get_triangle(hit.tri_ind).get_normal();
		float shading = rtm::dot(normal, ray.d);

		ray.o += hit.t * ray.d;
		ray.d = cosine_sample_hemisphere(global_data.normal_buffer[index]);
	}
	return 0;
}
*/

static uint32_t encode_pixel(rtm::vec3 in)
{
	in = rtm::clamp(in, 0.0f, 1.0f);
	uint32_t out = 0u;
	out |= static_cast<uint32_t>(in.r * 255.0f + 0.5f) << 0;
	out |= static_cast<uint32_t>(in.g * 255.0f + 0.5f) << 8;
	out |= static_cast<uint32_t>(in.b * 255.0f + 0.5f) << 16;
	out |= 0xff                                        << 24;
	return out;
}

void inline path_tracer()
{
	_KERNEL_AGRS;
	for(uint index = atomicinc(); index < global_data.framebuffer_size; index = atomicinc())
	{
		/*
		uint tile_index = index / global_data.tile_size;
		uint x = (tile_index % global_data.num_tiles_width) * global_data.tile_width;
		uint y = (tile_index / global_data.num_tiles_width) * global_data.tile_height;

		uint sub_tile_index = index % global_data.tile_size;
		x += sub_tile_index % global_data.tile_width;
		y += sub_tile_index / global_data.tile_width;
		*/

		uint x = index % global_data.framebuffer_width;
		uint y = index / global_data.framebuffer_width;
		RNG rng(index);

		rtm::vec3 output(0.0f);
		for(uint i = 0; i < global_data.samples_per_pixel; ++i)
		{
			Path path;
			for(uint j = 0; j < global_data.max_path_depth; ++j)
			{
				if(j == 0) global_data.camera.generate_ray_through_pixel(x, y, path.ray, rng);
				else       path.ray.d = cosine_sample_hemisphere(path.hit.normal, rng);
				path.ray.inv_d = rtm::vec3(1.0f) / path.ray.d;

				if(intersect(global_data.nodes, global_data.triangles, path.ray, false, path.hit))
				{
					path.throughput *= rtm::vec3(0.8f);
					path.ray.o += path.ray.d * path.hit.t;
					path.hit.t = T_MAX;

					//directional light
					float shading = rtm::dot(global_data.light_dir, path.hit.normal);
					if(shading > 0.0f)
					{
						Ray shadow_ray; Hit shadow_hit;
						shadow_ray.o = path.ray.o;
						shadow_ray.d = global_data.light_dir;
						shadow_ray.inv_d = rtm::vec3(1.0f) / shadow_ray.d;
						if(!intersect(global_data.nodes, global_data.triangles, shadow_ray, true, shadow_hit))
							path.radiance += path.throughput * rtm::vec3(0.8f, 0.75f, 0.7f) * shading;
					}
				}
				else
				{
					path.radiance += path.throughput * rtm::vec3(0.6f, 0.8f, 1.0f);
					break;
				}
			}
			output += path.radiance;
		}
		global_data.framebuffer[index] = encode_pixel(output * global_data.inverse_samples_per_pixel);
	}
}

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
	#if 0
	for(uint tile_index = atomicinc(); tile_index < global_data.num_tiles; tile_index = atomicinc())
	{
		uint tile_x = (tile_index % global_data.num_tiles_width) * global_data.tile_width;
		uint tile_y = (tile_index / global_data.num_tiles_width) * global_data.tile_height;

		for(uint sub_tile_index = 0; sub_tile_index < global_data.tile_size; ++sub_tile_index)
		{
			uint x = tile_x + sub_tile_index % global_data.tile_width;
			uint y = tile_y + sub_tile_index / global_data.tile_width;
			uint fb_index = x + y * global_data.framebuffer_width;
	#elif 0
	for(uint index = atomicinc(); index < global_data.framebuffer_size; index = atomicinc())
	{
		{
			uint tile_index = index / global_data.tile_size;
			uint sub_tile_index = index % global_data.tile_size;

			uint x = (tile_index % global_data.num_tiles_width) * global_data.tile_width;
			uint y = (tile_index / global_data.num_tiles_width) * global_data.tile_height;

			x += sub_tile_index % global_data.tile_width;
			y += sub_tile_index / global_data.tile_width;

			uint fb_index = x + y * global_data.framebuffer_width;
	#else 
	for(uint index = atomicinc(); index < global_data.framebuffer_size; index = atomicinc())
	{
		{
			uint fb_index = index;
			uint x = index % global_data.framebuffer_width;
			uint y = index / global_data.framebuffer_width;	
	#endif
			RNG rng(fb_index);

			rtm::vec3 output(0.0f);
			for(uint i = 0; i < global_data.samples_per_pixel; ++i)
			{
				Path path;
				for(uint j = 0; j < global_data.max_path_depth; ++j)
				{
					if(j == 0) global_data.camera.generate_ray_through_pixel(x, y, path.ray, rng);
					else       path.ray.d = cosine_sample_hemisphere(path.hit.normal, rng);

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
			global_data.framebuffer[fb_index] = encode_pixel(output * global_data.inverse_samples_per_pixel);
		}
	}
}

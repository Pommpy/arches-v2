#if 1

#include "stdafx.hpp"

//#include "trax.hpp"
#include "dual-streaming.hpp"

//global verbosity flag
int arches_verbosity = 1;

int main()
{
	//Arches::run_sim_trax();
	Arches::run_sim_dual_streaming();
	return 0;
}

#else
#include "../benchmarks/dual-streaming/src/stdafx.hpp"

#include "../benchmarks/dual-streaming/src/custom-instr.hpp"
#include "../benchmarks/dual-streaming/src/ray-tracing-include.hpp"

//replace arbitrary address with the global data we allocate when running nativly
GlobalData global_data;
#define _KERNEL_AGRS ;

int arches_verbosity = 1;

static uint32_t encode_pixel(rtm::vec3 in)
{
	in = rtm::clamp(in, 0.0f, 1.0f);
	uint32_t out = 0u;
	out |= static_cast<uint32_t>(in.r * 255.0f + 0.5f) << 0;
	out |= static_cast<uint32_t>(in.g * 255.0f + 0.5f) << 8;
	out |= static_cast<uint32_t>(in.b * 255.0f + 0.5f) << 16;
	out |= 0xff << 24;
	return out;
}

void static inline path_tracer()
{
	_KERNEL_AGRS;
	for(uint index = atomicinc(); index < global_data.framebuffer_size; index = atomicinc())
	{
		{
			uint fb_index = index;
			uint x = index % global_data.framebuffer_width;
			uint y = index / global_data.framebuffer_width;
			RNG rng(fb_index);

			rtm::vec3 output(0.0f);
			for(uint i = 0; i < global_data.samples_per_pixel; ++i)
			{
				Ray ray; Hit hit;
				rtm::vec3 attenuation(1.0f), radiance(0.0f);
				rtm::vec3 normal;
				for(uint j = 0; j < global_data.max_path_depth; ++j)
				{
					if(j == 0) global_data.camera.generate_ray_through_pixel(x, y, ray, rng);
					else       ray.d = cosine_sample_hemisphere(normal, rng);

					if(intersect(global_data.treelets, ray, false, hit))
					{
						normal = global_data.triangles[hit.prim_id].get_normal();

						attenuation *= rtm::vec3(0.8f);
						ray.o += ray.d * hit.t;
						hit.t = T_MAX;

						//directional light
						float shading = rtm::dot(global_data.light_dir, normal);
						if(shading > 0.0f)
						{
							//Ray shadow_ray; Hit shadow_hit;
							//shadow_ray.o = ray.o;
							//shadow_ray.d = global_data.light_dir;
							//if(!intersect(global_data.treelets, shadow_ray, true, shadow_hit))
							radiance += attenuation * rtm::vec3(0.8f, 0.75f, 0.7f) * shading;
						}
					}
					else
					{
						radiance += attenuation * rtm::vec3(0.6f, 0.8f, 1.0f);
						break;
					}
				}
				output += radiance;
			}
			global_data.framebuffer[fb_index] = encode_pixel(output * global_data.inverse_samples_per_pixel);
		}
	}
}

int main()
{
	Mesh mesh("./benchmarks/dual-streaming/res/sponza.obj");
	BVH bvh(mesh);
	TreeletBVH treelet_bvh(bvh, mesh);

	global_data.framebuffer_width = 1024;
	global_data.framebuffer_height = 1024;
	global_data.framebuffer_size = global_data.framebuffer_width * global_data.framebuffer_height;
	global_data.framebuffer = new uint32_t[global_data.framebuffer_size];

	global_data.tile_width = 8;
	global_data.tile_height = 8;
	global_data.tile_size = global_data.tile_width * global_data.tile_height;
	global_data.num_tiles_width = global_data.framebuffer_width / global_data.tile_width;
	global_data.num_tiles_height = global_data.framebuffer_height / global_data.tile_height;
	global_data.num_tiles = global_data.num_tiles_width * global_data.num_tiles_height;

	global_data.samples_per_pixel = 1;
	global_data.inverse_samples_per_pixel = 1.0f / global_data.samples_per_pixel;
	global_data.max_path_depth = 2;

	//global_data.camera = Camera(global_data.framebuffer_width, global_data.framebuffer_height, 60.0f, rtm::vec3(0.0f, 0.0f, 2.0f), rtm::vec3(0.0, 0.0, 0.0));
	global_data.camera = Camera(global_data.framebuffer_width, global_data.framebuffer_height, 90.0f, rtm::vec3(-900.6, 150.8, 120.74), rtm::vec3(79.7, 14.0, -17.4));
	global_data.light_dir = rtm::normalize(rtm::vec3(4.5, 42.5, 5.0));

	global_data.triangles = mesh.triangles;
	global_data.treelets = treelet_bvh.treelets.data();

	std::vector<std::thread*> threads(std::thread::hardware_concurrency(), nullptr);
	auto start = std::chrono::high_resolution_clock::now();

#if 0
	reset_atomicinc();
	for(auto& thread : threads) thread = new std::thread(path_tracer);
	for(std::thread* thread : threads) thread->join(), delete thread, 0;
#else
	path_tracer();
#endif

	auto stop = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
	std::cout << "Runtime: " << duration.count() << " ms\n\n";

	dump_framebuffer(global_data.framebuffer, "./out.ppm", global_data.framebuffer_width, global_data.framebuffer_height);

	delete[] global_data.framebuffer;
	return 0;
}

#endif
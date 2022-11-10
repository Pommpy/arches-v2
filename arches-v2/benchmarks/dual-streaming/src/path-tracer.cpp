#include "stdafx.hpp"

#include "custom-instr.hpp"
#include "ray-tracing-include.hpp"

#ifdef ARCH_X86
//replace arbitrary address with the global data we allocate when running nativly
GlobalData global_data;
#define _KERNEL_AGRS ;
#endif

#ifdef ARCH_RISCV
#define _KERNEL_AGRS const GlobalData& global_data = *reinterpret_cast<const GlobalData*>(GLOBAL_DATA_ADDRESS);
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

void static inline ray_gen()
{
	_KERNEL_AGRS;
	for(uint index = atomicinc(); index < global_data.framebuffer_size; index = atomicinc())
	{
		uint fb_index = index;
		uint x = index % global_data.framebuffer_width;
		uint y = index / global_data.framebuffer_width;	
		RNG rng(fb_index);	

		Ray ray; Hit hit;
		global_data.camera.generate_ray_through_pixel(x, y, ray, rng);

		global_data.ray_buffer[index] = ray;
		global_data.hit_buffer[index] = hit;
	}
}

void static inline trace_rays()
{
	_KERNEL_AGRS;
	for(uint index = atomicinc(); index < global_data.framebuffer_size; index = atomicinc())
	{
		uint fb_index = index;
		uint x = index % global_data.framebuffer_width;
		uint y = index / global_data.framebuffer_width;	
		RNG rng(fb_index);	

		Ray ray = global_data.ray_buffer[index];
		Hit hit = global_data.hit_buffer[index];
		intersect(global_data.treelets, ray, false, hit);
		global_data.hit_buffer[index] = hit;
	}
}

void static inline shade()
{
	_KERNEL_AGRS;
	for(uint index = atomicinc(); index < global_data.framebuffer_size; index = atomicinc())
	{
		uint fb_index = index;
		uint x = index % global_data.framebuffer_width;
		uint y = index / global_data.framebuffer_width;	
		RNG rng(fb_index);

		Ray ray = global_data.ray_buffer[index];
		Hit hit = global_data.hit_buffer[index];

		rtm::vec3 output(0.0f);
		if(hit.t < ray.t_max)
		{
			rtm::vec3 normal = global_data.triangles[hit.prim_id].get_normal();
			float shading = std::max(rtm::dot(global_data.light_dir, normal), 0.0f);
			output = rtm::vec3(0.8f * shading);
		}

		global_data.framebuffer[fb_index] = encode_pixel(output);
	}
}

void inline barrier()
{

}

void static inline path_tracer()
{
	ray_gen();
	barrier();
	trace_rays();
	barrier();
	shade();
}

#ifdef ARCH_RISCV
//gcc will only set main as entry point so we'll do this for now but we could theortically use path_tracer as entry
int main()
{
	path_tracer();
	return 0;
}
#endif

#ifdef ARCH_X86
int main()
{
	Mesh mesh("./res/sponza.obj");
	BVH bvh(mesh);
	TreeletBVH treelet_bvh(bvh, mesh);
	std::vector<Ray> ray_buffer(global_data.framebuffer_size);
	std::vector<Hit> hit_buffer(global_data.framebuffer_size);

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
	global_data.max_path_depth = 1;

	//global_data.camera = Camera(global_data.framebuffer_width, global_data.framebuffer_height, 60.0f, rtm::vec3(0.0f, 0.0f, 2.0f), rtm::vec3(0.0, 0.0, 0.0));
	global_data.camera = Camera(global_data.framebuffer_width, global_data.framebuffer_height, 90.0f, rtm::vec3(-900.6, 150.8, 120.74), rtm::vec3(79.7, 14.0, -17.4));
	global_data.light_dir = rtm::normalize(rtm::vec3(4.5, 42.5, 5.0));

	global_data.treelets = treelet_bvh.treelets.data();
	global_data.triangles = mesh.triangles;
	global_data.ray_buffer = ray_buffer.data();
	global_data.hit_buffer = hit_buffer.data();

	std::vector<std::thread*> threads(std::thread::hardware_concurrency(), nullptr);
	auto start = std::chrono::high_resolution_clock::now();

#if 1
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

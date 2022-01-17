#include "stdafx.hpp"

#include "kernels.hpp"


#ifdef ARCH_RISCV
int main()
{
	path_tracer();
	return 0;
}
#endif

#ifdef ARCH_X86
//replace arbitrary address with the global data we allocate when running nativly
GlobalData global_data;

int main()
{
	Mesh mesh("./res/sponza.obj");
	BVH bvh(mesh);

	global_data.framebuffer_width = 1024;
	global_data.framebuffer_height = 1024;
	global_data.framebuffer_size = global_data.framebuffer_width * global_data.framebuffer_height;
	global_data.framebuffer = new uint32_t[global_data.framebuffer_size];

	global_data.samples_per_pixel = 1;
	global_data.inverse_samples_per_pixel = 1.0f / global_data.samples_per_pixel;
	global_data.max_path_depth = 1;

	//global_data.camera = Camera(global_data.framebuffer_width, global_data.framebuffer_height, 60.0f, rtm::vec3(0.0f, 0.0f, 2.0f), rtm::vec3(0.0, 0.0, 0.0));
	global_data.camera = Camera(global_data.framebuffer_width, global_data.framebuffer_height, 90.0f, rtm::vec3(-900.6, 150.8, 120.74), rtm::vec3(79.7, 14.0, -17.4));
	global_data.light_dir = rtm::normalize(rtm::vec3(4.5, 42.5, 5.0));

	global_data.triangles = mesh.triangles;
	global_data.nodes = bvh.nodes;

	std::vector<std::thread*> threads(std::thread::hardware_concurrency(), nullptr);
	auto start = std::chrono::high_resolution_clock::now();

	{
		reset_atomicinc();
		for(auto& thread : threads) thread = new std::thread(path_tracer);
		for(std::thread* thread : threads) thread->join(), delete thread, 0;
	}

	auto stop = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
	std::cout << "Runtime: " << duration.count() << " ms\n\n";

	dump_framebuffer(global_data.framebuffer, "./out.ppm", global_data.framebuffer_width, global_data.framebuffer_height);

	delete[] global_data.framebuffer;
	return 0;
}
#endif

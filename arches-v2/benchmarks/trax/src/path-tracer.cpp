#include "stdafx.hpp"

#include "custom-instr.hpp"
#include "ray-tracing-include.hpp"

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

void static inline path_tracer(const GlobalData& global_data)
{
	for(uint index = atomicinc(); index < global_data.framebuffer_size; index = atomicinc())
	{
		uint32_t x = index % global_data.framebuffer_width;
		uint32_t y = index / global_data.framebuffer_width;	
		
		RNG rng(index);
		rtm::vec3 output(0.0f);
		for(uint i = 0; i < global_data.samples_per_pixel; ++i)
		{
			Ray ray = global_data.camera.generate_ray_through_pixel(x, y);
			Hit hit; hit.t = ray.t_max;

			rtm::vec3 attenuation(1.0f);
			for(uint j = 0; j < global_data.max_path_depth; ++j)
			{
				if(intersect(global_data.mesh, ray, hit)) 
				{
					rtm::uvec3 vi = global_data.mesh.vertex_indices[hit.id];
					rtm::vec3 e0 = global_data.mesh.vertices[vi[1]] - global_data.mesh.vertices[vi[0]];
					rtm::vec3 e1 = global_data.mesh.vertices[vi[2]] - global_data.mesh.vertices[vi[0]];
					rtm::vec3 n = rtm::normalize(rtm::cross(e0, e1));

					output += attenuation * rtm::vec3(0.8f) * rtm::vec3(1.0f, 0.8f, 0.6f) * std::max(0.0f, rtm::dot(n, global_data.light_dir));

					ray.o += ray.d * hit.t;
					ray.d = cosine_sample_hemisphere(n, rng);
					hit.t = ray.t_max;
					attenuation *= 0.8f / 3.1415926f;
				}
				else
				{
					output += attenuation * rtm::vec3(0.6f, 0.7f, 0.8f);
					break;
				}
			}
		}

		global_data.framebuffer[index] = encode_pixel(output * (1.0f / global_data.samples_per_pixel));
	}
}

#ifdef ARCH_RISCV
//gcc will only set main as entry point so we'll do this for now but we could theortically use path_tracer as entry
int main()
{
	path_tracer(*(GlobalData*)GLOBAL_DATA_ADDRESS);
	return 0;
}
#endif

#ifdef ARCH_X86
int main()
{
	GlobalData global_data;
	global_data.framebuffer_width = 1024;
	global_data.framebuffer_height = 1024;
	global_data.framebuffer_size = global_data.framebuffer_width * global_data.framebuffer_height;
	global_data.framebuffer = new uint32_t[global_data.framebuffer_size];

	global_data.samples_per_pixel = 4;
	global_data.max_path_depth = 4;

	global_data.light_dir = rtm::normalize(rtm::vec3(4.5f, 42.5f, 5.0f));

	global_data.camera = Camera(global_data.framebuffer_width, global_data.framebuffer_height, 24.0f, rtm::vec3(-900.6f, 150.8f, 120.74f), rtm::vec3(79.7f, 14.0f, -17.4f));
	Mesh mesh("./res/sponza.obj");
	BVH mesh_blas;
	std::vector<BuildObject> build_objects;
	for(uint i = 0; i < mesh.size(); ++i)
		build_objects.push_back(mesh.get_build_object(i));
	mesh_blas.build(build_objects);
	mesh.reorder(build_objects);

	global_data.mesh.blas = mesh_blas.nodes.data();
	global_data.mesh.vertex_indices = mesh.vertex_indices.data();
	global_data.mesh.vertices = mesh.vertices.data();

	auto start = std::chrono::high_resolution_clock::now();
	path_tracer(global_data);
	auto stop = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
	std::cout << "Runtime: " << duration.count() << " ms\n\n";

	dump_framebuffer(global_data.framebuffer, "./out.ppm", global_data.framebuffer_width, global_data.framebuffer_height);
	delete[] global_data.framebuffer;
	return 0;
}
#endif

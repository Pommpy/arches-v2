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
	for(uint32_t index = atomicinc(); index < global_data.framebuffer_size; index = atomicinc())
	{
		uint32_t x = index % global_data.framebuffer_width;
		uint32_t y = index / global_data.framebuffer_width;	

		Ray ray = global_data.camera.generate_ray_through_pixel(x, y);
		Hit hit; hit.t = ray.t_max;

		Ray last_ray = ray;
		uint last_patch_index = ~0u;

		RNG rng(index);

		glm::vec3 out(0.0f);
		glm::vec3 attenuation(1.0f);

		for(uint i = 0; 1; ++i)
		{
			if(intersect(global_data.tt2, 0.5f / 1.0f, last_patch_index, last_ray, ray, hit))
			{
				if(i >= 1) break;

				last_ray = ray;
				last_patch_index = hit.patch_index;

				rtm::uvec3 ni = global_data.ni[hit.id];
				rtm::vec3 n = rtm::normalize(global_data.normals[ni[0]] * hit.bc[0] + global_data.normals[ni[1]] * hit.bc[1] + global_data.normals[ni[2]] * (1.0f - hit.bc[0] - hit.bc[1])); 
				
				ray.o += ray.d * hit.t;
				ray.d = cosine_sample_hemisphere(n, rng);

				ray.radius += ray.drdt * hit.t;
				float roughness = 0.9f;
				float r2 = roughness * roughness;
				float a = fast_sqrtf(0.5f * (r2 / (1.0f - r2)));
				ray.drdt = 0.5f * a;

				attenuation *= 0.8f;
			}
			else
			{
				out += attenuation * 1.0f;
				break;
			}
		}

		global_data.framebuffer[index] = encode_pixel(out);
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
int main(int argc, char* argv[])
{	
	GlobalData global_data;
	global_data.framebuffer_width = 1024;
	global_data.framebuffer_height = 1024;
	global_data.framebuffer_size = global_data.framebuffer_width * global_data.framebuffer_height;
	global_data.framebuffer = new uint32_t[global_data.framebuffer_size];

	global_data.camera = Camera(global_data.framebuffer_width, global_data.framebuffer_height, 35.0f, rtm::vec3(0.0f, 0.0f, 4.0f));
	
	Mesh mesh(std::string(argv[1]) + ".obj");
	TesselationTree2 tt(std::string(argv[1]) + ".tt2");

	BVH blas;
	std::vector<BuildObject> build_objects;
	for(uint i = 0; i < tt.size(); ++i)
		build_objects.push_back(tt.get_build_object(i));
	blas.build(build_objects);
	tt.reorder(build_objects);

	global_data.tt2.blas = blas.nodes.data();
	global_data.tt2.headers = tt.headers.data();
	global_data.tt2.nodes = tt.nodes.data();
	global_data.tt2.vertices = tt.vertices.data();

	global_data.ni = mesh.normal_indices.data();
	global_data.normals = mesh.normals.data();

	auto start = std::chrono::high_resolution_clock::now();
	path_tracer(global_data);
	auto stop = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
	std::cout << "Runtime: " << duration.count() << " ms\n\n";

	dump_framebuffer(global_data.framebuffer, "./out-tt2.ppm", global_data.framebuffer_width, global_data.framebuffer_height);
	delete[] global_data.framebuffer;

	return 0;
}
#endif

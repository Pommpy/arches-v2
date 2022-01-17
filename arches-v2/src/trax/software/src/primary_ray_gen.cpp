#include "stdafx.hpp"

#include "custom-instr.hpp"
#include "ray-tracing-include.hpp"

#ifdef ARCH_RISCV
#define _KERNEL_AGRS GlobalData& global_data = *reinterpret_cast<GlobalData*>(GLOBAL_DATA_ADDRESS); Mesh mesh(global_data.tris); MeshBVH bvh(&mesh, global_data.nodes);
#endif

int main()
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
	}
	return 0;
}

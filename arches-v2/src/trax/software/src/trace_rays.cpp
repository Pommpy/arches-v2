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
		Hit hit;
		Ray ray = global_data.ray_buffer[index];
		bvh.intersect(ray, hit, false);
		global_data.hit_buffer[index] = hit;
		global_data.radiance_buffer[index] = rtm::vec3(hit.t / 1024.0f);
	}
	return 0;
}

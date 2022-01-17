#include "stdafx.hpp"

#include "arches/simulator/simulator.hpp"

#include "arches/units/unit-core-simple.hpp"
#include "arches/units/unit-sram.hpp"

#include "arches/util/elf.hpp"

#include "trax/software/src/ray-tracing-include.hpp"

using namespace Arches;

void run_sim_simple()
{
	Simulator simulator;
	Units::UnitSRAM       sram(1024ull * 1024ull * 1024ull, 1, &simulator);
	Units::UnitCoreSimple core(&sram, &simulator);
	core.backing_memory = sram._data_u8;

	sram.clear();
	ELF elf("res/test-programs/gradient");
	sram.write_elf(elf);
	core.pc = elf.elf_header->e_entry.u64;


	auto start = std::chrono::high_resolution_clock::now();

	simulator.execute();

	auto stop = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
	std::cout << "Runtime: " << duration.count() << " ms\n";


	std::cout << "Cycles: " << simulator.current_cycle << "\n";

	physical_address paddr_frame_buffer = 0x11138ull;
	sram.dump_as_ppm_uint8(paddr_frame_buffer, 1024, 1024, "out.ppm");

}

GlobalData initilize_buffers(Units::UnitMainMemoryBase* main_memory, physical_address& heap_address)
{
	Mesh mesh("src/trax/software/res/sponza.obj");
	BVH  bvh(mesh);

	GlobalData global_data;
	global_data.framebuffer_width = 32;
	global_data.framebuffer_height = 32;
	global_data.framebuffer_size = global_data.framebuffer_width * global_data.framebuffer_height;
	global_data.framebuffer = reinterpret_cast<uint32_t*>(heap_address); heap_address += global_data.framebuffer_size * sizeof(uint32_t);

	global_data.samples_per_pixel = 1;
	global_data.inverse_samples_per_pixel = 1.0f / global_data.samples_per_pixel;
	global_data.max_path_depth = 1;

	//global_data.camera = Camera(global_data.framebuffer_width, global_data.framebuffer_height, 60.0f, rtm::vec3(0.0f, 0.0f, 2.0f), rtm::vec3(0.0, 0.0, 0.0));
	global_data.camera = Camera(global_data.framebuffer_width, global_data.framebuffer_height, 90.0f, rtm::vec3(-900.6, 150.8, 120.74), rtm::vec3(79.7, 14.0, -17.4));
	global_data.light_dir = rtm::normalize(rtm::vec3(4.5, 42.5, 5.0));

	main_memory->direct_write(mesh.triangles, mesh.num_triangles * sizeof(Triangle), heap_address);
	global_data.triangles = reinterpret_cast<Triangle*>(heap_address); heap_address += mesh.num_triangles * sizeof(Triangle);

	main_memory->direct_write(bvh.nodes, bvh.num_nodes * sizeof(Node), heap_address);
	global_data.nodes = reinterpret_cast<Node*>(heap_address); heap_address += bvh.num_nodes * sizeof(Node);

	main_memory->direct_write(&global_data, sizeof(GlobalData), GLOBAL_DATA_ADDRESS);

	return global_data;
}

void run_sim_trax()
{
	Simulator simulator;
	ELF elf("src/trax/software/bin/riscv/trax");

	Units::UnitSRAM sram(1024ull * 1024ull * 1024ull, 1024, &simulator); sram.clear();
	physical_address heap_address = sram.write_elf(elf);
	GlobalData global_data = initilize_buffers(&sram, heap_address);

	std::vector<Units::UnitCoreSimple*> cores;
	uint32_t TRAX_STACK_SIZE = 4096;
	physical_address stack_start = 1024 * 1024 * 1024 - TRAX_STACK_SIZE;
	for(uint i = 0; i < 1024; ++i)
	{
		Units::UnitCoreSimple* core = new Units::UnitCoreSimple(&sram, &simulator);
		core->int_regs->sp.u64 = stack_start; stack_start -= TRAX_STACK_SIZE;
		core->pc = elf.elf_header->e_entry.u64;
		core->backing_memory = sram._data_u8;
		core->core_id = i;
		cores.push_back(core);
	}

	{
		auto start = std::chrono::high_resolution_clock::now();

		simulator.execute();

		auto stop = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
		std::cout << "Runtime: " << duration.count() << " ms\n";
		std::cout << "Cycles: " << simulator.current_cycle << "\n";
	}

	for(auto& core : cores)
		delete core;

	physical_address paddr_frame_buffer = reinterpret_cast<physical_address>(global_data.framebuffer);
	sram.dump_as_ppm_uint8(paddr_frame_buffer, global_data.framebuffer_width, global_data.framebuffer_height, "out.ppm");
}

int main()
{
	run_sim_trax();
		
	return 0;
}
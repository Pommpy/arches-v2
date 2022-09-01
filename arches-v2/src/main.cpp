#include "stdafx.hpp"

#include "arches/simulator/simulator.hpp"

#include "arches/units/unit-core-simple.hpp"
#include "arches/units/unit-sram.hpp"
#include "arches/units/unit-dram.hpp"
#include "arches/units/uint-atomic-increment.hpp"
#include "arches/units/unit-cache.hpp"

#include "arches/util/elf.hpp"

#include "software/trax/src/ray-tracing-include.hpp"

using namespace Arches;

//global verbosity flag
int arches_verbosity = 1;

void run_sim_simple()
{
	Simulator simulator;
	Units::UnitSRAM            sram(1024ull * 1024ull * 1024ull, &simulator);
	Units::UnitAtomicIncrement amoin(&simulator);
	Units::UnitCoreSimple core(&sram, &sram, &amoin, &simulator);
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

	paddr_t paddr_frame_buffer = 0x11138ull;
	sram.dump_as_ppm_uint8(paddr_frame_buffer, 1024, 1024, "out.ppm");

}

paddr_t aligne_to_cache_line(paddr_t paddr)
{
	return (paddr + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1);
}

GlobalData initilize_buffers(Units::UnitMainMemoryBase* main_memory, paddr_t& heap_address)
{
	Mesh mesh("src/software/trax/res/sponza.obj");
	BVH  bvh(mesh);

	GlobalData global_data;
	global_data.framebuffer_width = 1024;
	global_data.framebuffer_height = 1024;
	global_data.framebuffer_size = global_data.framebuffer_width * global_data.framebuffer_height;
	heap_address = aligne_to_cache_line(heap_address);
	global_data.framebuffer = reinterpret_cast<uint32_t*>(heap_address); heap_address += global_data.framebuffer_size * sizeof(uint32_t);

	global_data.tile_width = 32;
	global_data.tile_height = 32;
	global_data.tile_size = global_data.tile_width * global_data.tile_height;
	global_data.num_tiles_width = global_data.framebuffer_width / global_data.tile_width;
	global_data.num_tiles_height = global_data.framebuffer_height / global_data.tile_height;
	global_data.num_tiles = global_data.num_tiles_width * global_data.num_tiles_height;

	global_data.samples_per_pixel = 1;
	global_data.inverse_samples_per_pixel = 1.0f / global_data.samples_per_pixel;
	global_data.max_path_depth = 1;

	//global_data.camera = Camera(global_data.framebuffer_width, global_data.framebuffer_height, 60.0f, rtm::vec3(0.0f, 0.0f, 2.0f), rtm::vec3(0.0, 0.0, 0.0));
	global_data.camera = Camera(global_data.framebuffer_width, global_data.framebuffer_height, 90.0f, rtm::vec3(-900.6f, 150.8f, 120.74f), rtm::vec3(79.7f, 14.0f, -17.4f));
	global_data.light_dir = rtm::normalize(rtm::vec3(4.5f, 42.5f, 5.0f));

	heap_address = aligne_to_cache_line(heap_address);
	main_memory->direct_write(mesh.triangles, mesh.num_triangles * sizeof(Triangle), heap_address);
	global_data.triangles = reinterpret_cast<Triangle*>(heap_address); heap_address += mesh.num_triangles * sizeof(Triangle);

	heap_address = aligne_to_cache_line(heap_address);
	main_memory->direct_write(bvh.nodes, bvh.num_nodes * sizeof(Node), heap_address);
	global_data.nodes = reinterpret_cast<Node*>(heap_address); heap_address += bvh.num_nodes * sizeof(Node);

	main_memory->direct_write(&global_data, sizeof(GlobalData), GLOBAL_DATA_ADDRESS);

	return global_data;
}

void run_sim_trax()
{
	Simulator simulator;
	ELF elf("src/software/trax/bin/riscv/trax");

	//Units::UnitSRAM sram(1024ull * 1024ull * 1024ull, &simulator); sram.clear();
	Units::UnitDRAM mm(1024ull * 1024ull * 1024ull, &simulator); mm.clear();
	Units::UnitAtomicIncrement amoin(&simulator);
	paddr_t heap_address = mm.write_elf(elf);
	GlobalData global_data = initilize_buffers(&mm, heap_address);

	std::vector<Units::UnitCoreSimple*> cores;
	std::vector<Units::UnitCache*> l1s;
	std::vector<Units::UnitCache*> l2s;

	uint num_tps_per_tm = 32;
	uint num_tms_per_l2 = 8;
	uint num_l2 = 4;

	Units::UnitCache::CacheConfiguration l1_config;
	l1_config.associativity = 1;
	l1_config.bank_stride = 1;
	l1_config.cache_size = 32 * 1024;
	l1_config.line_size = CACHE_LINE_SIZE;
	l1_config.num_banks = 8;
	l1_config.penalty = 1;

	Units::UnitCache::CacheConfiguration l2_config;
	l2_config.associativity = 1;
	l2_config.bank_stride = 1;
	l2_config.cache_size = 512 * 1024;
	l2_config.line_size = CACHE_LINE_SIZE;
	l2_config.num_banks = 16;
	l2_config.penalty = 3;
	
#if 1
	for(uint k = 0; k < num_l2; ++k)
	{
		Units::UnitCache* l2 = new Units::UnitCache(l2_config, &mm, &simulator);
		l2s.push_back(l2);

		for(uint j = 0; j < num_tms_per_l2; ++j)
		{
			Units::UnitCache* l1 = new Units::UnitCache(l1_config, l2, &simulator);
			l1s.push_back(l1);

			for(uint i = 0; i < num_tps_per_tm; ++i)
			{
				Units::UnitCoreSimple* core = new Units::UnitCoreSimple(l1, &mm, &amoin, &simulator);
				cores.push_back(core);

				core->int_regs->sp.u64 = 0ull;
				core->pc = elf.elf_header->e_entry.u64;
				core->backing_memory = mm._data_u8;
			}
		}
	}
#else
	Units::UnitCoreSimple* core = new Units::UnitCoreSimple(&sram, &sram, &amoin, &simulator);
	cores.push_back(core);

	core->int_regs->sp.u64 = stack_start; stack_start -= TRAX_STACK_SIZE;
	core->pc = elf.elf_header->e_entry.u64;
	core->backing_memory = sram._data_u8;

#endif

	cycles_t warm_up_end = 0;
	{
		auto start = std::chrono::high_resolution_clock::now();
		simulator.execute();
		auto stop = std::chrono::high_resolution_clock::now();
		
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
		std::cout << "Runtime: " << duration.count() << " ms\n";
		std::cout << "Cycles: " << simulator.current_cycle << "\n";
		warm_up_end = simulator.current_cycle;
	}

#if 0
	for (auto& core : cores)
	{
		core->int_regs->sp.u64 = 0ull;
		core->int_regs->ra.u64 = 0ull;
		core->pc = elf.elf_header->e_entry.u64;
		core->executing = true;
	}
	amoin.counter = 0u;

	{
		auto start = std::chrono::high_resolution_clock::now();
		simulator.execute();
		auto stop = std::chrono::high_resolution_clock::now();

		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
		std::cout << "Runtime: " << duration.count() << " ms\n";
		std::cout << "Cycles: " << (simulator.current_cycle - warm_up_end) << "\n";
	}
#endif

	printf("\nCore0\n");
	cores[0]->log.print_log();
	printf("\nL10\n");
	l1s[0]->log.print_log();
	printf("\nL20\n");
	l2s[0]->log.print_log();

	mm.print_usimm_stats(l2_config.line_size, 4, simulator.current_cycle);

	for(auto& core : cores)
		delete core;

	for(auto& l1 : l1s)
		delete l1;

	for(auto& l2 : l2s)
		delete l2;

	paddr_t paddr_frame_buffer = reinterpret_cast<paddr_t>(global_data.framebuffer);
	mm.dump_as_png_uint8(paddr_frame_buffer, global_data.framebuffer_width, global_data.framebuffer_height, "out.png");
}

int main()
{
	run_sim_trax();
		
	return 0;
}
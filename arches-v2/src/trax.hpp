#pragma once

#include "arches/simulator/simulator.hpp"

#include "arches/units/unit-dram.hpp"
#include "arches/units/unit-cache.hpp"
#include "arches/units/uint-atomic-increment.hpp"
#include "arches/units/unit-core-simple.hpp"

#include "arches/units/unit-sfu.hpp"
#include "arches/units/trax/unit-trax-tp.hpp"

#include "arches/util/elf.hpp"

#include "../benchmarks/trax/src/ray-tracing-include.hpp"

namespace Arches {

static paddr_t align_to(size_t alignment, paddr_t paddr)
{
	return (paddr + alignment - 1) & ~(alignment - 1);
}

static GlobalData initilize_buffers(Units::UnitMainMemoryBase* main_memory, paddr_t& heap_address)
{
	Mesh mesh("benchmarks/trax/res/sponza.obj");
	BVH  bvh(mesh);

	GlobalData global_data;
	global_data.framebuffer_width = 64;
	global_data.framebuffer_height = 64;
	global_data.framebuffer_size = global_data.framebuffer_width * global_data.framebuffer_height;
	heap_address = align_to(4 * 1024, heap_address);
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

	heap_address = align_to(CACHE_LINE_SIZE, heap_address);
	main_memory->direct_write(mesh.triangles, mesh.num_triangles * sizeof(Triangle), heap_address);
	global_data.triangles = reinterpret_cast<Triangle*>(heap_address); heap_address += mesh.num_triangles * sizeof(Triangle);

	heap_address = align_to(CACHE_LINE_SIZE, heap_address); heap_address += sizeof(Node);
	main_memory->direct_write(bvh.nodes, bvh.num_nodes * sizeof(Node), heap_address);
	global_data.nodes = reinterpret_cast<Node*>(heap_address); heap_address += bvh.num_nodes * sizeof(Node);

	main_memory->direct_write(&global_data, sizeof(GlobalData), GLOBAL_DATA_ADDRESS);

	return global_data;
}

static void run_sim_trax()
{
	uint num_tps_per_tm = 32;
	uint num_tms_per_l2 = 8;
	uint num_l2 = 4;

	uint num_tps = num_tps_per_tm * num_tms_per_l2 * num_l2;
	uint num_tms = num_tms_per_l2 * num_l2;
	uint num_sfus = static_cast<uint>(ISA::RISCV::Type::NUM_TYPES) * num_tms;

	Units::UnitCache::Configuration l1_config;
	l1_config.associativity = 4;
	l1_config.cache_size = 32 * 1024;
	l1_config.line_size = CACHE_LINE_SIZE;
	l1_config.num_banks = 8;
	l1_config.num_incoming_connections = num_tps_per_tm;
	l1_config.penalty = 1;
	l1_config.blocking = false;
	l1_config.num_mshr = 8;
	l1_config.num_lse = 32;

	Units::UnitCache::Configuration l2_config;
	l2_config.associativity = 4;
	l2_config.cache_size = 512 * 1024;
	l2_config.line_size = CACHE_LINE_SIZE;
	l2_config.num_banks = 16;
	l2_config.num_incoming_connections = num_tms_per_l2 * l1_config.num_banks;
	l2_config.penalty = 3;
	l2_config.blocking = true;

	Simulator simulator;
	ELF elf("benchmarks/trax/bin/riscv/path-tracer");

	//Units::UnitSRAM sram(1024ull * 1024ull * 1024ull, &simulator); sram.clear();
	Units::UnitDRAM mm(l2_config.num_banks * num_l2, 1024ull * 1024ull * 1024ull, &simulator); mm.clear();
	Units::UnitAtomicRegfile amoin(num_tps_per_tm * num_tms_per_l2 * num_l2, &simulator);
	paddr_t heap_address = mm.write_elf(elf);
	GlobalData global_data = initilize_buffers(&mm, heap_address);

	simulator.register_unit(&mm);
	simulator.register_unit(&amoin);

	//std::vector<Units::UnitCoreSimple*> cores;
	std::vector<Units::TRaX::UnitTP*> tps;
	std::vector<Units::UnitCache*> l1s;
	std::vector<Units::UnitCache*> l2s;

	std::vector<Units::UnitSFU*> sfus;
	std::vector<std::vector<Units::UnitSFU*>> sfus_tables;

	Units::TRaX::UnitTP::Configuration tp_config;
	tp_config.atomic_inc = &amoin;
	tp_config.main_mem = &mm;
	tp_config.tm_index = num_tps_per_tm;

	paddr_t stack_start = 1024ull * 1024ull * 1024ull;

	for(uint k = 0; k < num_l2; ++k)
	{
		uint l2_mm_ports = k * l2_config.num_banks;
		Units::UnitCache* l2 = new Units::UnitCache(l2_config, &mm, l2_mm_ports, &simulator);
		l2s.push_back(l2);

		//simulator.start_new_unit_group();
		//simulator.register_unit(l2);

		for(uint j = 0; j < num_tms_per_l2; ++j)
		{
			simulator.start_new_unit_group();

			if(j == 0) simulator.register_unit(l2s.back());

			uint l1_l2_ports = j * l1_config.num_banks;
			Units::UnitCache* l1 = new Units::UnitCache(l1_config, l2, l1_l2_ports, &simulator);
			l1s.push_back(l1);

			simulator.register_unit(l1);

			sfus_tables.emplace_back(static_cast<uint>(ISA::RISCV::Type::NUM_TYPES), nullptr);
			Units::UnitSFU** sfu_table = sfus_tables.back().data();
			tp_config.mem_higher = l1;
			tp_config.sfu_table = sfu_table;

			sfus.push_back(_new Units::UnitSFU(8, 1, 2, num_tps_per_tm, &simulator));
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FADD)] = sfus.back();
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FSUB)] = sfus.back();
			simulator.register_unit(sfus.back());

			sfus.push_back(_new Units::UnitSFU(8, 1, 2, num_tps_per_tm, &simulator));
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FMUL)] = sfus.back();
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FFMAD)] = sfus.back();
			simulator.register_unit(sfus.back());

			sfus.push_back(_new Units::UnitSFU(2, 1, 1, num_tps_per_tm, &simulator));
			sfu_table[static_cast<uint>(ISA::RISCV::Type::IMUL)] = sfus.back();
			sfu_table[static_cast<uint>(ISA::RISCV::Type::IDIV)] = sfus.back();
			simulator.register_unit(sfus.back());

			sfus.push_back(_new Units::UnitSFU(1, 1, 20, num_tps_per_tm, &simulator));
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FDIV)] = sfus.back();
			simulator.register_unit(sfus.back());

			sfus.push_back(_new Units::UnitSFU(1, 1, 20, num_tps_per_tm, &simulator));
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FSQRT)] = sfus.back();
			simulator.register_unit(sfus.back());

			for(uint i = 0; i < num_tps_per_tm; ++i)
			{
				tp_config.global_index = (k * num_tms_per_l2 + j) * num_tps_per_tm + i;
				tp_config.tm_index = i;

				Units::TRaX::UnitTP* tp = new Units::TRaX::UnitTP(tp_config, &simulator);
				tps.push_back(tp);
				tp->int_regs->sp.u64 = stack_start; stack_start -= 1024;
				tp->stack_end = stack_start;
				tp->pc = elf.elf_header->e_entry.u64;
				tp->backing_memory = mm._data_u8;

				simulator.register_unit(tp);
			}
		}
	}

	{
		auto start = std::chrono::high_resolution_clock::now();
		simulator.execute();
		auto stop = std::chrono::high_resolution_clock::now();

		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
		std::cout << "Runtime: " << duration.count() << " ms\n";
		std::cout << "Cycles: " << simulator.current_cycle << "\n";
	}

	//printf("\nCore0\n");
	//cores[0]->log.print_log();
	printf("\nTP\n");
	Units::TRaX::UnitTP::Log tp_log;
	for(auto& tp : tps)
		tp_log.accumulate(tp->log);
	tp_log.print_log();

	printf("\nL1\n");
	Units::UnitCache::Log l1_log;
	for(auto& l1 : l1s)
		l1_log.accumulate(l1->log);
	l1_log.print_log();

	printf("\nL2\n");
	Units::UnitCache::Log l2_log;
	for(auto& l2 : l2s)
		l2_log.accumulate(l2->log);
	l2_log.print_log(l2s.size());

	mm.print_usimm_stats(l2_config.line_size, 4, simulator.current_cycle);

	//for(auto& core : cores)
	//	delete core;

	for(auto& tp : tps)
		delete tp;

	for(auto& sfu : sfus)
		delete sfu;

	for(auto& l1 : l1s)
		delete l1;

	for(auto& l2 : l2s)
		delete l2;

	paddr_t paddr_frame_buffer = reinterpret_cast<paddr_t>(global_data.framebuffer);
	mm.dump_as_png_uint8(paddr_frame_buffer, global_data.framebuffer_width, global_data.framebuffer_height, "out.png");
}

}
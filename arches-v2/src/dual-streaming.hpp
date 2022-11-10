#pragma once

#include "arches/simulator/simulator.hpp"

#include "arches/units/unit-dram.hpp"
#include "arches/units/unit-cache.hpp"
#include "arches/units/uint-atomic-increment.hpp"
#include "arches/units/unit-core-simple.hpp"
#include "arches/units/unit-sfu.hpp"

#include "arches/util/elf.hpp"

#include "arches/units/dual-streaming/unit-ds-tp.hpp"

#include "../benchmarks/dual-streaming/src/ray-tracing-include.hpp"

namespace Arches
{
	static paddr_t align_to(size_t alignment, paddr_t paddr)
	{
		return (paddr + alignment - 1) & ~(alignment - 1);
	}

	static GlobalData initilize_buffers(Units::UnitMainMemoryBase* main_memory, paddr_t& heap_address)
	{
		Mesh mesh("benchmarks/dual-streaming/res/sponza.obj");
		BVH  bvh(mesh);
		TreeletBVH treelet_bvh(bvh, mesh);

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

		heap_address = align_to(sizeof(Treelet), heap_address); heap_address += sizeof(Treelet);
		main_memory->direct_write(treelet_bvh.treelets.data(), treelet_bvh.treelets.size() * sizeof(Treelet), heap_address);
		global_data.treelets = reinterpret_cast<Treelet*>(heap_address); heap_address += treelet_bvh.treelets.size() * sizeof(Treelet);

		main_memory->direct_write(&global_data, sizeof(GlobalData), GLOBAL_DATA_ADDRESS);

		return global_data;
	}

	static void run_sim_dual_streaming()
	{
		uint num_tps_per_tm = 16;
		uint num_tms_per_l2 = 128;
		uint num_l2 = 1;

		uint num_tps = num_tps_per_tm * num_tms_per_l2 * num_l2;
		uint num_tms = num_tms_per_l2 * num_l2;
		uint num_sfus = static_cast<uint>(ISA::RISCV::Type::NUM_TYPES) * num_tms;

		uint stack_size = 1024;
		uint global_data_size = 64 * 1024;
		uint binary_size = 64 * 1024;
		uint ray_stageing_buffer_size = 4 * 1024;
		uint scene_buffer_size = 4 * 1024 * 1024;
		uint hit_record_buffer_size = 4 * 1024 * 1024; //TODO how big is this actually?

		size_t mem_size = 8ull * 1024 * 1024 * 1024; //8GB

		size_t dsmm_global_data_start        = 0x0ull;
		size_t dsmm_binary_start             = dsmm_global_data_start        + global_data_size;
		size_t dsmm_ray_staging_buffer_start = dsmm_binary_start             + binary_size;
		size_t dsmm_scene_buffer_start       = dsmm_ray_staging_buffer_start + ray_stageing_buffer_size * num_tms;
		size_t dsmm_hit_record_start         = dsmm_scene_buffer_start       + scene_buffer_size;
		size_t dsmm_heap_start               = dsmm_hit_record_start         + hit_record_buffer_size;
		size_t dsmm_stack_start              = mem_size                      - stack_size * num_tps;

		Units::UnitCache::Configuration l1_config;
		l1_config.associativity = 1;
		l1_config.cache_size = 16 * 1024;
		l1_config.line_size = CACHE_LINE_SIZE;
		l1_config.num_banks = 8;
		l1_config.num_incoming_connections = num_tps_per_tm;
		l1_config.penalty = 1;
		l1_config.blocking = false;
		l1_config.num_mshr = 8;
		l1_config.num_lse = 32;

		Units::UnitCache::Configuration l2_config;
		l2_config.associativity = 1;
		l2_config.cache_size = 512 * 1024;
		l2_config.line_size = CACHE_LINE_SIZE;
		l2_config.num_banks = 32;
		l2_config.num_incoming_connections = num_tms_per_l2 * l1_config.num_banks;
		l2_config.penalty = 3;
		l2_config.blocking = true;

		Simulator simulator;

		Units::UnitDRAM mm(l2_config.num_banks * num_l2, mem_size, &simulator); mm.clear();
		simulator.register_unit(&mm);

		ELF elf("benchmarks/dual-streaming/bin/riscv/path-tracer");
		mm.write_elf(elf);

		paddr_t heap_address = dsmm_heap_start;
		GlobalData global_data = initilize_buffers(&mm, heap_address);

		Units::UnitAtomicIncrement amoin(num_tps_per_tm * num_tms_per_l2 * num_l2, &simulator);
		simulator.register_unit(&amoin);

		std::vector<Units::DualStreaming::UnitTP*> tps;
		std::vector<Units::UnitCache*> l1s;
		std::vector<Units::UnitCache*> l2s;
		std::vector<Units::UnitSFU*> sfus;

		std::vector<std::vector<Units::UnitSFU*>> sfus_tables;

		std::vector<Units::MemoryUnitMap> tp_mem_maps;
		std::vector<Units::MemoryUnitMap> l1_mem_maps;

		Units::DualStreaming::UnitTP::Configuration tp_config;
		tp_config.atomic_inc = &amoin;
		tp_config.main_mem = &mm;
		tp_config.tm_index = num_tps_per_tm;

		paddr_t sp = dsmm_stack_start;

		for(uint k = 0; k < num_l2; ++k)
		{
			uint l2_mm_ports = k * l2_config.num_banks;
			Units::UnitCache* l2 = _new Units::UnitCache(l2_config, &mm, l2_mm_ports, &simulator);
			l2s.push_back(l2);

			simulator.start_new_unit_group();
			simulator.register_unit(l2);

			for(uint j = 0; j < num_tms_per_l2; ++j)
			{
				simulator.start_new_unit_group();

				uint l1_l2_ports = j * l1_config.num_banks;
				Units::UnitCache* l1 = _new Units::UnitCache(l1_config, l2, l1_l2_ports, &simulator);
				l1s.push_back(l1);

				simulator.register_unit(l1);

				tp_mem_maps.emplace_back();
				tp_mem_maps.back().add_unit(dsmm_global_data_start, l1);
				tp_mem_maps.back().add_unit(dsmm_binary_start, l1);
				tp_mem_maps.back().add_unit(dsmm_ray_staging_buffer_start, l1);
				tp_mem_maps.back().add_unit(dsmm_scene_buffer_start, l1);
				tp_mem_maps.back().add_unit(dsmm_hit_record_start, l1);
				tp_mem_maps.back().add_unit(dsmm_heap_start, l1);
				tp_mem_maps.back().add_unit(dsmm_stack_start, nullptr);

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

				sfus.push_back(_new Units::UnitSFU(1, 1, 8, num_tps_per_tm, &simulator));
				sfu_table[static_cast<uint>(ISA::RISCV::Type::CUSTOM1)] = sfus.back();
				simulator.register_unit(sfus.back());

				sfus.push_back(_new Units::UnitSFU(2, 18, 31, num_tps_per_tm, &simulator));
				sfu_table[static_cast<uint>(ISA::RISCV::Type::CUSTOM2)] = sfus.back();
				simulator.register_unit(sfus.back());

				for(uint i = 0; i < num_tps_per_tm; ++i)
				{
					tp_config.global_index = (k * num_tms_per_l2 + j) * num_tps_per_tm + i;
					tp_config.tm_index = i;

					Units::DualStreaming::UnitTP* tp = new Units::DualStreaming::UnitTP(tp_config, &simulator);
					tps.push_back(tp);

					sp += stack_size;
					tp->int_regs->sp.u64 = sp;

					tp->stack_start = dsmm_stack_start;
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

		printf("\nTP\n");
		Units::DualStreaming::UnitTP::Log tp_log;
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

		paddr_t paddr_frame_buffer = reinterpret_cast<paddr_t>(global_data.framebuffer);
		mm.dump_as_png_uint8(paddr_frame_buffer, global_data.framebuffer_width, global_data.framebuffer_height, "./out.png");
	
		for(auto& tp : tps) delete tp;
		for(auto& l1 : l1s) delete l1;
		for(auto& l2 : l2s) delete l2;
		for(auto& sfu : sfus) delete sfu;
	}
}
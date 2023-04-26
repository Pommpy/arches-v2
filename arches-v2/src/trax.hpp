#pragma once

#include "arches/simulator/simulator.hpp"

#include "arches/units/unit-dram.hpp"
#include "arches/units/unit-cache.hpp"
#include "arches/units/uint-atomic-reg-file.hpp"
#include "arches/units/uint-tile-scheduler.hpp"

#include "arches/units/unit-sfu.hpp"
#include "arches/units/unit-tp.hpp"

#include "arches/util/elf.hpp"

#include "../benchmarks/trax/src/ray-tracing-include.hpp"


namespace Arches {

namespace ISA { namespace RISCV { namespace TRaX {

//TRAXAMOIN
const static InstructionInfo traxamoin(0b00010, "traxamoin", Type::FCHTHRD, Encoding::U, RegFile::INT, IMPL_DECL
{
	unit->mem_req.type = Units::MemoryRequest::Type::FCHTHRD;
	unit->mem_req.dst.reg = instr.i.rd;
	unit->mem_req.dst.reg_file = 0;
	unit->mem_req.dst.sign_ext = 0;
	unit->mem_req.size = 4;
	unit->mem_req.vaddr = 0x0ull;
});

}}}

static paddr_t align_to(size_t alignment, paddr_t paddr)
{
	return (paddr + alignment - 1) & ~(alignment - 1);
}

template <typename T>
static T* write_array(Units::UnitMainMemoryBase* main_memory, size_t alignment, T* data, size_t size, paddr_t& heap_address)
{
	paddr_t array_address = align_to(alignment, heap_address);
	heap_address = array_address + size * sizeof(T);
	main_memory->direct_write(data, size * sizeof(T), array_address);
	return reinterpret_cast<T*>(array_address);
}

template <typename T>
static T* write_vector(Units::UnitMainMemoryBase* main_memory, size_t alignment, std::vector<T> v, paddr_t& heap_address)
{
	return write_array(main_memory, alignment, v.data(), v.size(), heap_address);
}

static GlobalData initilize_buffers(Units::UnitMainMemoryBase* main_memory, paddr_t& heap_address)
{
	GlobalData global_data;
	global_data.framebuffer_width = 1024;
	global_data.framebuffer_height = 1024;
	global_data.framebuffer_size = global_data.framebuffer_width * global_data.framebuffer_height;

	heap_address = align_to(4 * 1024, heap_address);
	global_data.framebuffer = reinterpret_cast<uint32_t*>(heap_address); heap_address += global_data.framebuffer_size * sizeof(uint32_t);

	global_data.samples_per_pixel = 1;
	global_data.max_bounces = 0;

	global_data.light_dir = rtm::normalize(rtm::vec3(4.5f, 42.5f, 5.0f));
	global_data.camera = Camera(global_data.framebuffer_width, global_data.framebuffer_height, 12.0f, rtm::vec3(-900.6f, 150.8f, 120.74f), rtm::vec3(79.7f, 14.0f, -17.4f));
	//global_data.camera = Camera(global_data.framebuffer_width, global_data.framebuffer_height, 24.0f, rtm::vec3(0.0f, 0.0f, 5.0f));

	Mesh mesh("benchmarks/trax/res/sponza.obj");
	BVH blas;
	std::vector<BuildObject> build_objects;
	for(uint i = 0; i < mesh.size(); ++i)
		build_objects.push_back(mesh.get_build_object(i));
	blas.build(build_objects);
	mesh.reorder(build_objects);

	global_data.mesh.blas = write_vector(main_memory, CACHE_BLOCK_SIZE, blas.nodes, heap_address);
	global_data.mesh.vertex_indices = write_vector(main_memory, CACHE_BLOCK_SIZE, mesh.vertex_indices, heap_address);
	global_data.mesh.vertices = write_vector(main_memory, CACHE_BLOCK_SIZE, mesh.vertices, heap_address);

	main_memory->direct_write(&global_data, sizeof(GlobalData), GLOBAL_DATA_ADDRESS);

	return global_data;
}

static void run_sim_trax(int argc, char* argv[])
{
	uint num_tps_per_tm = 32;
	uint num_tms_per_l2 = 8;
	uint num_l2 = 4;

	uint num_tps = num_tps_per_tm * num_tms_per_l2 * num_l2;
	uint num_tms = num_tms_per_l2 * num_l2;
	uint sfu_table_size = static_cast<uint>(ISA::RISCV::Type::NUM_TYPES);

	//hardware spec
	uint64_t mem_size = 4ull * 1024ull * 1024ull * 1024ull; //4GB

	//cached global data
	uint64_t stack_size = 2048; //2KB
	uint64_t global_data_size = 64 * 1024; //64KB for global data
	uint64_t binary_size = 64 * 1024; //64KB for executable data

	//glboal data
	vaddr_t mm_null_address = 0x0ull;
	vaddr_t mm_global_data_start = GLOBAL_DATA_ADDRESS;
	vaddr_t mm_binary_start = mm_global_data_start + global_data_size;

	vaddr_t mm_heap_start = mm_binary_start + binary_size;

	vaddr_t mm_stack_start = mem_size - stack_size;
	vaddr_t stack_pointer = mem_size;

	ISA::RISCV::isa[ISA::RISCV::CUSTOM_OPCODE0] = ISA::RISCV::TRaX::traxamoin;

	Simulator simulator;

	std::vector<Units::UnitTP*> tps;
	std::vector<Units::UnitCache*> l1s;
	std::vector<Units::UnitCache*> l2s;
	std::vector<Units::UnitSFU*> sfus;

	std::vector<Units::UnitSFU*> sfu_tables(sfu_table_size * num_tms);
	
	Units::UnitDRAM mm(num_l2, 1024ull * 1024ull * 1024ull, &simulator); mm.clear();
	simulator.register_unit(&mm);
	
	ELF elf("benchmarks/trax/bin/riscv/path-tracer");
	vaddr_t global_pointer;
	paddr_t heap_address = mm.write_elf(elf);
	
	GlobalData global_data = initilize_buffers(&mm, heap_address);

	Units::UnitTileScheduler tile_scheduler(num_tps, num_tms, global_data.framebuffer_width, global_data.framebuffer_height, 8, 8, &simulator);
	simulator.register_unit(&tile_scheduler);

	for(uint l2_index = 0; l2_index < num_l2; ++l2_index)
	{
		Units::UnitCache::Configuration l2_config;
		l2_config.size = 512 * 1024;
		l2_config.associativity = 1;
		l2_config.penalty = 3;
		l2_config.num_ports = num_tms_per_l2;
		l2_config.num_banks = 16;
		l2_config.num_lfb = 4;
		l2_config.mem_map.add_unit(mm_null_address, &mm, l2_index, 1);

		l2s.push_back(new Units::UnitCache(l2_config));

		for(uint tm_i = 0; tm_i < num_tms_per_l2; ++tm_i)
		{
			simulator.start_new_unit_group();
			if(tm_i == 0) simulator.register_unit(l2s.back());

			uint tm_index = l2_index * num_tms_per_l2 + tm_i;

			Units::UnitCache::Configuration l1_config;
			l1_config.size = 32 * 1024;
			l1_config.associativity = 1;
			l1_config.penalty = 1;
			l1_config.num_ports = num_tps_per_tm;
			l1_config.port_size = 16;
			l1_config.num_banks = 8;
			l1_config.num_lfb = 4;
			l1_config.mem_map.add_unit(mm_null_address, l2s.back(), tm_i, 1);

			l1s.push_back(new Units::UnitCache(l1_config));
			simulator.register_unit(l1s.back());

			Units::UnitSFU** sfu_table = &sfu_tables[sfu_table_size * tm_index];

			sfus.push_back(_new Units::UnitSFU(16, 1, 2, num_tps_per_tm));
			simulator.register_unit(sfus.back());
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FADD)] = sfus.back();
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FMUL)] = sfus.back();
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FFMAD)] = sfus.back();

			sfus.push_back(_new Units::UnitSFU(2, 1, 1, num_tps_per_tm));
			simulator.register_unit(sfus.back());
			sfu_table[static_cast<uint>(ISA::RISCV::Type::IMUL)] = sfus.back();
			sfu_table[static_cast<uint>(ISA::RISCV::Type::IDIV)] = sfus.back();

			sfus.push_back(_new Units::UnitSFU(1, 1, 20, num_tps_per_tm));
			simulator.register_unit(sfus.back());
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FDIV)] = sfus.back();

			sfus.push_back(_new Units::UnitSFU(1, 1, 20, num_tps_per_tm));
			simulator.register_unit(sfus.back());
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FSQRT)] = sfus.back();

			for(uint tp_index = 0; tp_index < num_tps_per_tm; ++tp_index)
			{
				Units::UnitTP::Configuration tp_config;
				tp_config.tp_index = tp_index;
				tp_config.tm_index = tm_index;
				tp_config.pc = elf.elf_header->e_entry.u64;
				tp_config.sp = stack_pointer;
				tp_config.stack_size = stack_size;
				tp_config.backing_memory = mm._data_u8;
				tp_config.sfu_table = sfu_table;
				tp_config.port_size = 16;
				tp_config.mem_map.add_unit(mm_null_address, &tile_scheduler, tm_index * num_tps_per_tm + tp_index, 1);
				tp_config.mem_map.add_unit(mm_global_data_start, l1s.back(), tp_index, 1);
				tp_config.mem_map.add_unit(mm_stack_start, nullptr, 0, 0);

				tps.push_back(new Units::UnitTP(tp_config));
				simulator.register_unit(tps.back());
				simulator.units_executing++;
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
	Units::UnitTP::Log tp_log(elf.segments[0]->vaddr);
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
	l2_log.print_log();

	mm.print_usimm_stats(CACHE_BLOCK_SIZE, 4, simulator.current_cycle);

	for(auto& tp : tps)
		delete tp;

	for(auto& sfu : sfus)
		delete sfu;

	for(auto& l1 : l1s)
		delete l1;

	for(auto& l2 : l2s)
		delete l2;

	paddr_t paddr_frame_buffer = reinterpret_cast<paddr_t>(global_data.framebuffer);
	stbi_flip_vertically_on_write(true);
	mm.dump_as_png_uint8(paddr_frame_buffer, global_data.framebuffer_width, global_data.framebuffer_height, "out.png");
}

}
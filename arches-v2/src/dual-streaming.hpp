#pragma once

#include "arches/simulator/simulator.hpp"

#include "arches/units/unit-dram.hpp"
#include "arches/units/unit-cache.hpp"
#include "arches/units/unit-buffer.hpp"
#include "arches/units/unit-atomic-reg-file.hpp"
#include "arches/units/unit-tile-scheduler.hpp"
#include "arches/units/unit-sfu.hpp"

#include "arches/units/unit-tp.hpp"

#include "arches/units/dual-streaming/unit-stream-scheduler.hpp"
#include "arches/units/dual-streaming/unit-ray-staging-buffer.hpp"

#include "arches/util/elf.hpp"
#include "arches/isa/riscv.hpp"

#include "../benchmarks/dual-streaming/src/ray-tracing-include.hpp"

namespace Arches {

namespace ISA { namespace RISCV {

//see the opcode map for details
const static InstructionInfo isa_custom0_000_imm[8] =
{
	InstructionInfo(0x0, "fchthrd", Type::FCHTHRD, Encoding::U, RegFile::INT, IMPL_DECL
	{
		unit->mem_req.type = Units::MemoryRequest::Type::FCHTHRD;
		unit->mem_req.dst.reg_file = 0;
		unit->mem_req.dst.reg = instr.i.rd;
		unit->mem_req.dst.sign_ext = 0;
		unit->mem_req.size = 4;
	}),
	InstructionInfo(0x1, "boxisect", Type::BOXISECT, Encoding::U, RegFile::FLOAT, IMPL_DECL
	{
		Register32 * fr = unit->float_regs->registers;

		rtm::vec3 inv_d;

		Ray ray;
		ray.o.x = fr[0].f32;
		ray.o.y = fr[1].f32;
		ray.o.z = fr[2].f32;
		inv_d.x = fr[3].f32;
		inv_d.y = fr[4].f32;
		inv_d.z = fr[5].f32;

		AABB aabb;
		aabb.min.x = fr[6].f32;
		aabb.min.y = fr[7].f32;
		aabb.min.z = fr[8].f32;
		aabb.max.x = fr[9].f32;
		aabb.max.y = fr[10].f32;
		aabb.max.z = fr[11].f32;

		unit->float_regs->registers[instr.u.rd].f32 = intersect(aabb, ray, inv_d);
	}),
	InstructionInfo(0x2, "triisect", Type::TRIISECT, Encoding::U, RegFile::FLOAT, IMPL_DECL
	{
		Register32 * fr = unit->float_regs->registers;

		Ray ray;
		ray.o.x = fr[0].f32;
		ray.o.y = fr[1].f32;
		ray.o.z = fr[2].f32;
		ray.d.x = fr[3].f32;
		ray.d.y = fr[4].f32;
		ray.d.z = fr[5].f32;

		Triangle tri;
		tri.vrts[0].x = fr[6].f32;
		tri.vrts[0].y = fr[7].f32;
		tri.vrts[0].z = fr[8].f32;
		tri.vrts[1].x = fr[9].f32;
		tri.vrts[1].y = fr[10].f32;
		tri.vrts[1].z = fr[11].f32;
		tri.vrts[2].x = fr[12].f32;
		tri.vrts[2].y = fr[13].f32;
		tri.vrts[2].z = fr[14].f32;

		Hit hit;
		hit.t = fr[15].f32;
		hit.bc[0] = fr[16].f32;
		hit.bc[1] = fr[17].f32;

		unit->int_regs->registers[instr.u.rd].u32 = intersect(tri, ray, hit);

		fr[15].f32 = hit.t;
		fr[16].f32 = hit.bc[0];
		fr[17].f32 = hit.bc[1];
	}),
};

const static InstructionInfo isa_custom0_funct3[8] =
{
	InstructionInfo(0x0, META_DECL{return isa_custom0_000_imm[instr.u.imm_31_12 >> 3]; }),
	InstructionInfo(0x1, "lbray", Type::LBRAY, Encoding::I, RegFile::FLOAT, IMPL_DECL
	{	
		//load bucket ray into registers f0 - f8
		unit->mem_req.type = Units::MemoryRequest::Type::LBRAY;
		unit->mem_req.size = sizeof(BucketRay);

		unit->mem_req.dst.reg = 0;
		unit->mem_req.dst.reg_file = static_cast<uint8_t>(RegFile::FLOAT);
		unit->mem_req.dst.sign_ext = 0;

		unit->mem_req.vaddr = unit->int_regs->registers[instr.i.rs1].u64 + i_imm(instr);
	}),
	InstructionInfo(0x2, "sbray", Type::SBRAY, Encoding::S, RegFile::INT, IMPL_DECL
	{
		//store bucket ray to hit record updater
		Register32* fr = unit->float_regs->registers;

		unit->mem_req.type = Units::MemoryRequest::Type::SBRAY;
		unit->mem_req.size = sizeof(BucketRay);

		unit->mem_req.vaddr = unit->int_regs->registers[instr.s.rs1].u64 + s_imm(instr);
		
		Ray ray;
		ray.o.x = fr[0].f32;
		ray.o.y = fr[1].f32;
		ray.o.z = fr[2].f32;
		ray.d.x = fr[3].f32;
		ray.d.y = fr[4].f32;
		ray.d.z = fr[5].f32;
	}),
	InstructionInfo(0x3, "cshit", Type::CSHIT, Encoding::S, RegFile::INT, IMPL_DECL
	{	
		Register32* fr = unit->float_regs->registers;

		Hit hit;
		hit.t       = fr[15].f32;
		hit.bc[0]   = fr[16].f32;
		hit.bc[1]   = fr[17].f32;
		hit.prim_id = fr[18].u32;
	}),
};

const static InstructionInfo custom0(CUSTOM_OPCODE0, META_DECL{return isa_custom0_funct3[instr.i.funct3]; });

}}

static paddr_t align_to(size_t alignment, paddr_t paddr)
{
	return (paddr + alignment - 1) & ~(alignment - 1);
}

static void initilize_buffers(GlobalData& global_data, Units::UnitMainMemoryBase* main_memory, paddr_t& heap_address)
{
	Mesh mesh("benchmarks/dual-streaming/res/sponza.obj");
	BVH  bvh(mesh);
	TreeletBVH treelet_bvh(bvh, mesh);


	global_data.framebuffer_width = 64;
	global_data.framebuffer_height = 64;
	global_data.framebuffer_size = global_data.framebuffer_width * global_data.framebuffer_height;
	heap_address = align_to(8 * 1024, heap_address);
	global_data.hit_records = reinterpret_cast<Hit*>(heap_address); heap_address += global_data.framebuffer_size * sizeof(Hit);
	heap_address = align_to(8 * 1024, heap_address);
	global_data.framebuffer = reinterpret_cast<uint32_t*>(heap_address); heap_address += global_data.framebuffer_size * sizeof(uint32_t);

	global_data.samples_per_pixel = 1;
	global_data.inverse_samples_per_pixel = 1.0f / global_data.samples_per_pixel;
	global_data.max_path_depth = 1;

	//global_data.camera = Camera(global_data.framebuffer_width, global_data.framebuffer_height, 60.0f, rtm::vec3(0.0f, 0.0f, 2.0f), rtm::vec3(0.0, 0.0, 0.0));
	global_data.camera = Camera(global_data.framebuffer_width, global_data.framebuffer_height, 90.0f, rtm::vec3(-900.6f, 150.8f, 120.74f), rtm::vec3(79.7f, 14.0f, -17.4f));
	global_data.light_dir = rtm::normalize(rtm::vec3(4.5f, 42.5f, 5.0f));

	heap_address = align_to(CACHE_BLOCK_SIZE, heap_address);
	main_memory->direct_write(mesh.triangles, mesh.num_triangles * sizeof(Triangle), heap_address);
	global_data.triangles = reinterpret_cast<Triangle*>(heap_address); heap_address += mesh.num_triangles * sizeof(Triangle);

	heap_address = align_to(sizeof(Treelet), heap_address); heap_address += sizeof(Treelet);
	main_memory->direct_write(treelet_bvh.treelets.data(), treelet_bvh.treelets.size() * sizeof(Treelet), heap_address);
	global_data.treelets = reinterpret_cast<Treelet*>(heap_address); heap_address += treelet_bvh.treelets.size() * sizeof(Treelet);

	main_memory->direct_write(&global_data, sizeof(GlobalData), GLOBAL_DATA_ADDRESS);
}

static void run_sim_dual_streaming(int argc, char* argv[])
{
	uint64_t num_tps_per_tm = 1;
	uint64_t num_tms = 1;

	uint64_t num_tps = num_tps_per_tm * num_tms;
	uint64_t num_sfus = static_cast<uint>(ISA::RISCV::Type::NUM_TYPES) * num_tms;

	//hardware spec
	uint64_t mem_size = 4ull * 1024ull * 1024ull * 1024ull; //4GB
	
	//cached global data
	uint64_t stack_size = 1024; //1KB
	uint64_t global_data_size = 64 * 1024; //64KB for global data
	uint64_t binary_size = 64 * 1024; //64KB for executable data

	//mem mapped buffers
	uint64_t ray_stageing_buffer_size = 2 * 1024; //2KB ray-staging buffers
	uint64_t scene_buffer_size = 4 * 1024 * 1024; //4MB scene buffer

	//glboal data
	vaddr_t dsmm_null_address          = 0x0ull;
	vaddr_t dsmm_atomic_reg_file_start = ATOMIC_REG_FILE_ADDRESS;
	vaddr_t dsmm_global_data_start     = GLOBAL_DATA_ADDRESS;
	vaddr_t dsmm_binary_start          = dsmm_global_data_start + global_data_size;

	vaddr_t dsmm_ray_staging_buffer_start = dsmm_binary_start + binary_size;
	vaddr_t dsmm_scene_buffer_start       = scene_buffer_size;
	vaddr_t dsmm_mem_mapped_buffers_end   = dsmm_scene_buffer_start + scene_buffer_size;

	vaddr_t dsmm_heap_start = dsmm_mem_mapped_buffers_end;

	vaddr_t dsmm_stack_start = mem_size - stack_size * num_tps;
	vaddr_t stack_pointer = dsmm_stack_start;

	ISA::RISCV::isa[ISA::RISCV::CUSTOM_OPCODE0] = ISA::RISCV::custom0;

	Simulator simulator;

	std::vector<Units::UnitTP*> tps;
	std::vector<Units::UnitSFU*> sfus;
	std::vector<Units::DualStreaming::UnitRayStagingBuffer*> rsbs;
	std::vector<Units::UnitThreadScheduler*> thread_schedulers;
	std::vector<Units::UnitCache*> l1s;
	std::vector<std::vector<Units::UnitSFU*>> sfus_tables;

	Units::UnitDRAM mm(2, mem_size, &simulator); mm.clear();
	simulator.register_unit(&mm);

	simulator.start_new_unit_group();

	ELF elf("benchmarks/dual-streaming/bin/riscv/path-tracer");
	mm.write_elf(elf);

	paddr_t heap_address = dsmm_heap_start;
	GlobalData global_data;
	global_data.scene_buffer = *(Treelet**)&dsmm_scene_buffer_start;
	global_data.ray_staging_buffer = *(Treelet**)&dsmm_ray_staging_buffer_start;
	initilize_buffers(global_data, &mm, heap_address);

	Units::DualStreaming::UnitStreamScheduler::Configuration stream_scheduler_config;
	stream_scheduler_config.scene_start = *(paddr_t*)&global_data.scene_buffer;
	stream_scheduler_config.bucket_start = *(paddr_t*)&heap_address;
	stream_scheduler_config.num_tms = num_tms;
	stream_scheduler_config.bucket_size = 2 * 1024;
	stream_scheduler_config.ray_size = 32;
	stream_scheduler_config.segment_size = 64 * 1024;

	Units::DualStreaming::UnitStreamScheduler stream_scheduler(stream_scheduler_config);
	simulator.register_unit(&stream_scheduler);

	Units::UnitBuffer::Configuration scene_buffer_config;
	scene_buffer_config.size = scene_buffer_size;
	scene_buffer_config.num_banks = 32;
	scene_buffer_config.num_ports = num_tms + 1;
	scene_buffer_config.penalty = 3;

	Units::UnitBuffer scene_buffer(scene_buffer_config);
	simulator.register_unit(&scene_buffer);

	simulator.start_new_unit_group();

	Units::UnitCache::Configuration l2_config;
	l2_config.size = 512 * 1024;
	l2_config.associativity = 1;
	l2_config.num_banks = 32;
	l2_config.num_ports = num_tms;
	l2_config.penalty = 3;
	l2_config.num_lfb = 4;

	l2_config.mem_map.add_unit(0x0ull, &mm, 0, 1);

	Units::UnitCache l2(l2_config);
	simulator.register_unit(&l2);

	Units::UnitAtomicRegfile atomic_regs(num_tms);
	simulator.register_unit(&atomic_regs);

	for(uint tm_index = 0; tm_index < num_tms; ++tm_index)
	{
		simulator.start_new_unit_group();

		Units::UnitCache::Configuration l1_config;
		l1_config.size = 16 * 1024;
		l1_config.associativity = 1;
		l1_config.num_banks = 8;
		l1_config.num_ports = num_tps_per_tm;
		l1_config.penalty = 1;
		l1_config.num_lfb = 4;

		l1_config.mem_map.add_unit(dsmm_null_address, &l2, tm_index, 1);
		l1_config.mem_map.add_unit(dsmm_scene_buffer_start, &scene_buffer, tm_index, 1);
		l1_config.mem_map.add_unit(dsmm_heap_start, &l2, tm_index, 1);

		Units::UnitCache* l1 = _new Units::UnitCache(l1_config);
		l1s.push_back(l1);
		simulator.register_unit(l1);

		Units::DualStreaming::UnitRayStagingBuffer* rsb = _new Units::DualStreaming::UnitRayStagingBuffer(num_tps, tm_index, &stream_scheduler);
		rsbs.push_back(rsb);
		simulator.register_unit(rsb);
		
		sfus_tables.emplace_back(static_cast<uint>(ISA::RISCV::Type::NUM_TYPES), nullptr);
		Units::UnitSFU** sfu_table = sfus_tables.back().data();

		sfus.push_back(_new Units::UnitSFU(16, 1, 2, num_tps_per_tm));
		sfu_table[static_cast<uint>(ISA::RISCV::Type::FADD)] = sfus.back();
		sfu_table[static_cast<uint>(ISA::RISCV::Type::FMUL)] = sfus.back();
		sfu_table[static_cast<uint>(ISA::RISCV::Type::FFMAD)] = sfus.back();
		simulator.register_unit(sfus.back());

		sfus.push_back(_new Units::UnitSFU(2, 1, 1, num_tps_per_tm));
		sfu_table[static_cast<uint>(ISA::RISCV::Type::IMUL)] = sfus.back();
		sfu_table[static_cast<uint>(ISA::RISCV::Type::IDIV)] = sfus.back();
		simulator.register_unit(sfus.back());

		sfus.push_back(_new Units::UnitSFU(1, 1, 20, num_tps_per_tm));
		sfu_table[static_cast<uint>(ISA::RISCV::Type::FDIV)] = sfus.back();
		simulator.register_unit(sfus.back());

		sfus.push_back(_new Units::UnitSFU(1, 1, 20, num_tps_per_tm));
		sfu_table[static_cast<uint>(ISA::RISCV::Type::FSQRT)] = sfus.back();
		simulator.register_unit(sfus.back());

		sfus.push_back(_new Units::UnitSFU(1, 1, 8, num_tps_per_tm));
		sfu_table[static_cast<uint>(ISA::RISCV::Type::BOXISECT)] = sfus.back();
		simulator.register_unit(sfus.back());

		sfus.push_back(_new Units::UnitSFU(2, 18, 31, num_tps_per_tm));
		sfu_table[static_cast<uint>(ISA::RISCV::Type::TRIISECT)] = sfus.back();
		simulator.register_unit(sfus.back());

		thread_schedulers.push_back(_new  Units::UnitThreadScheduler(num_tps_per_tm, tm_index, &atomic_regs));
		simulator.register_unit(thread_schedulers.back());

		for(uint tp_index = 0; tp_index < num_tps_per_tm; ++tp_index)
		{
			Units::UnitTP::Configuration tp_config;
			tp_config.tp_index = tp_index;
			tp_config.tm_index = tm_index;
			tp_config.pc = elf.elf_header->e_entry.u64;
			tp_config.sp = stack_pointer;
			tp_config.backing_memory = mm._data_u8;
			tp_config.sfu_table = sfu_table;
			tp_config.port_size = 32;

			tp_config.mem_map.add_unit(dsmm_null_address, nullptr, 0, 1);
			tp_config.mem_map.add_unit(dsmm_atomic_reg_file_start, thread_schedulers.back(), tm_index * num_tps_per_tm + tp_index, 1);
			tp_config.mem_map.add_unit(dsmm_global_data_start, l1, tp_index, 1);
			tp_config.mem_map.add_unit(dsmm_ray_staging_buffer_start, rsb, tp_index, 1);
			tp_config.mem_map.add_unit(dsmm_scene_buffer_start, l1, tp_index, 1); //scene buffer data goes through l1  
			//tp_config.mem_map.add_unit(dsmm_heap_start, l1, tp_index);
			tp_config.mem_map.add_unit(dsmm_stack_start, nullptr, 0, 1);

			Units::UnitTP* tp = new Units::UnitTP(tp_config);
			tps.push_back(tp);

			simulator.register_unit(tp);
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
	Units::UnitTP::Log tp_log(0x10000);
	for(auto& tp : tps)
		tp_log.accumulate(tp->log);
	tp_log.print_log();

	printf("\nL1\n");
	Units::UnitCache::Log l1_log;
	for(auto& l1 : l1s)
		l1_log.accumulate(l1->log);
	l1_log.print_log();

	printf("\nL2\n");
	l2.log.print_log();

	mm.print_usimm_stats(CACHE_BLOCK_SIZE, 4, simulator.current_cycle);

	paddr_t paddr_frame_buffer = reinterpret_cast<paddr_t>(global_data.framebuffer);
	mm.dump_as_png_uint8(paddr_frame_buffer, global_data.framebuffer_width, global_data.framebuffer_height, "./out.png");

	for(auto& tp : tps) delete tp;
	for(auto& sfu : sfus) delete sfu;
	for(auto& rsb : rsbs) delete rsb;
	for(auto& ts : thread_schedulers) delete ts;
	for(auto& l1 : l1s) delete l1;

}

}
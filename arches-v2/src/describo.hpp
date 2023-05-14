#pragma once

#include "arches/simulator/simulator.hpp"

#include "arches/units/unit-dram.hpp"
#include "arches/units/unit-cache.hpp"
#include "arches/units/uint-atomic-reg-file.hpp"
#include "arches/units/uint-tile-scheduler.hpp"

#include "arches/units/unit-sfu.hpp"
#include "arches/units/unit-tp.hpp"

#include "arches/util/elf.hpp"

#include "../benchmarks/describo/src/ray-tracing-include.hpp"

#include "arches/isa/riscv.hpp"

namespace Arches {

namespace ISA { namespace RISCV { namespace DTRaX {
	const static InstructionInfo isa_custom0[8] =
	{
	InstructionInfo(0x1, "fchthrd", Type::FCHTHRD, Encoding::U, RegFile::INT, IMPL_DECL
	{
		unit->mem_req.type = Units::MemoryRequest::Type::FCHTHRD;
		unit->mem_req.size = 4;

		unit->mem_req.dst.reg = instr.i.rd;
		unit->mem_req.dst.reg_file = 0;
		unit->mem_req.dst.sign_ext = false;

		unit->mem_req.vaddr = 0;
	}),
	InstructionInfo(0x2, "boxisect", Type::BOXISECT, Encoding::U, RegFile::FLOAT, IMPL_DECL
	{
		Register32 * fr = unit->float_regs->registers;

		rtm::vec3 inv_d;

		Ray ray;
		ray.o.x = fr[3].f32;
		ray.o.y = fr[4].f32;
		ray.o.z = fr[5].f32;
		ray.t_min = fr[6].f32;
		inv_d.x = fr[7].f32;
		inv_d.y = fr[8].f32;
		inv_d.z = fr[9].f32;
		ray.t_max = fr[10].f32;

		ray.d = rtm::vec3(1.0f) / inv_d;

		AABB aabb;
		aabb.min.x = fr[11].f32;
		aabb.min.y = fr[12].f32;
		aabb.min.z = fr[13].f32;
		aabb.max.x = fr[14].f32;
		aabb.max.y = fr[15].f32;
		aabb.max.z = fr[16].f32;

		unit->float_regs->registers[instr.u.rd].f32 = intersect(aabb, ray, inv_d);
	}),
	InstructionInfo(0x3, "triisect", Type::TRIISECT, Encoding::U, RegFile::INT, IMPL_DECL
	{
		Register32 * fr = unit->float_regs->registers;

		Hit hit;
		hit.t = fr[0].f32;

		Ray ray;
		ray.o.x = fr[3].f32;
		ray.o.y = fr[4].f32;
		ray.o.z = fr[5].f32;
		ray.t_min = fr[6].f32;
		ray.d.x = fr[7].f32;
		ray.d.y = fr[8].f32;
		ray.d.z = fr[9].f32;
		ray.t_max = fr[10].f32;

		Triangle tri;
		tri.vrts[0].x = fr[11].f32;
		tri.vrts[0].y = fr[12].f32;
		tri.vrts[0].z = fr[13].f32;
		tri.vrts[1].x = fr[14].f32;
		tri.vrts[1].y = fr[15].f32;
		tri.vrts[1].z = fr[16].f32;
		tri.vrts[2].x = fr[17].f32;
		tri.vrts[2].y = fr[18].f32;
		tri.vrts[2].z = fr[19].f32;

		unit->int_regs->registers[instr.u.rd].u32 = intersect(tri, ray, hit);

		fr[0].f32 = hit.t;
		fr[1].f32 = hit.bc[0];
		fr[2].f32 = hit.bc[1];
	}),
	};

	const static InstructionInfo custom0(CUSTOM_OPCODE0, META_DECL{return isa_custom0[instr.u.imm_31_12 >> 3]; });

}}}

static paddr_t align_to(size_t alignment, paddr_t paddr)
{
	return (paddr + alignment - 1) & ~(alignment - 1);
}

template <typename RET>
static RET* write_array(Units::UnitMainMemoryBase* main_memory, size_t alignment, RET* data, size_t size, paddr_t& heap_address)
{
	paddr_t array_address = align_to(alignment, heap_address);
	heap_address = array_address + size * sizeof(RET);
	main_memory->direct_write(data, size * sizeof(RET), array_address);
	return reinterpret_cast<RET*>(array_address);
}

template <typename RET>
static RET* write_vector(Units::UnitMainMemoryBase* main_memory, size_t alignment, std::vector<RET> v, paddr_t& heap_address)
{
	return write_array(main_memory, alignment, v.data(), v.size(), heap_address);
}

static GlobalData initilize_buffers(std::string exe_path, std::string mesh_path, rtm::vec3 camera_pos, Units::UnitMainMemoryBase* main_memory, paddr_t& heap_address)
{
	GlobalData global_data;
	global_data.framebuffer_width = 1024;
	global_data.framebuffer_height = 1024;
	global_data.framebuffer_size = global_data.framebuffer_width * global_data.framebuffer_height;

	heap_address = align_to(8 * 1024, heap_address);
	global_data.framebuffer = reinterpret_cast<uint32_t*>(heap_address); heap_address += global_data.framebuffer_size * sizeof(uint32_t);

	Camera camera(global_data.framebuffer_width, global_data.framebuffer_height, 35.0f, camera_pos);
	//Camera camera(global_data.framebuffer_width, global_data.framebuffer_height, 35.0f, rtm::vec3(0.0f, 0.0f, 6.0f));
	global_data.camera = camera;

	Mesh mesh(mesh_path + ".obj");
	if(exe_path.back() == '1')
	{
		TesselationTree1 tt(mesh_path + ".tt1");

		BVH blas;
		std::vector<BuildObject> build_objects;
		for(uint i = 0; i < tt.size(); ++i)
			build_objects.push_back(tt.get_build_object(i));
		blas.build(build_objects);
		tt.reorder(build_objects);

		printf("Headers: %lld\n", tt.headers.size());
		printf("Vertices: %lld\n", tt.vertices.size());
		printf("Triangles: %lld\n", tt.triangles.size());
		printf("Nodes: %lld\n", tt.nodes.size() + blas.nodes.size());
		printf("Total Size: %lld bytes\n",
			tt.vertices.size() * sizeof(rtm::vec3) +
			tt.headers.size() * sizeof(TesselationTree1::Header) +
			tt.nodes.size() * sizeof(TesselationTree1::Node) +
			tt.triangles.size() * sizeof(CompactTri) +
			blas.nodes.size() * sizeof(BVH::Node)
		);

		global_data.tt1.blas = write_vector(main_memory, CACHE_BLOCK_SIZE, blas.nodes, heap_address);
		global_data.tt1.headers = write_vector(main_memory, CACHE_BLOCK_SIZE, tt.headers, heap_address);
		global_data.tt1.nodes = write_vector(main_memory, CACHE_BLOCK_SIZE, tt.nodes, heap_address);
		global_data.tt1.vertices = write_vector(main_memory, CACHE_BLOCK_SIZE, tt.vertices, heap_address);
		global_data.tt1.triangles = write_vector(main_memory, CACHE_BLOCK_SIZE, tt.triangles, heap_address);
	}
	else if(exe_path.back() == '2')
	{
		TesselationTree2 tt(mesh_path + ".tt2");

		BVH blas;
		std::vector<BuildObject> build_objects;
		for(uint i = 0; i < tt.size(); ++i)
			build_objects.push_back(tt.get_build_object(i));
		blas.build(build_objects);
		tt.reorder(build_objects);

		printf("Headers: %lld\n", tt.headers.size());
		printf("Vertices: %lld\n", tt.vertices.size());
		printf("Nodes: %lld\n", tt.nodes.size() + blas.nodes.size());
		printf("Total Size: %lld bytes\n",
			tt.vertices.size() * sizeof(rtm::vec3) +
			tt.headers.size() * sizeof(TesselationTree2::Header) +
			tt.nodes.size() * sizeof(TesselationTree2::Node) +
			blas.nodes.size() * sizeof(BVH::Node)
		);

		global_data.tt2.blas = write_vector(main_memory, CACHE_BLOCK_SIZE, blas.nodes, heap_address);
		global_data.tt2.headers = write_vector(main_memory, CACHE_BLOCK_SIZE, tt.headers, heap_address);
		global_data.tt2.nodes = write_vector(main_memory, CACHE_BLOCK_SIZE, tt.nodes, heap_address);
		global_data.tt2.vertices = write_vector(main_memory, CACHE_BLOCK_SIZE, tt.vertices, heap_address);
	}
	else if(exe_path.back() == '3')
	{
		TesselationTree3 tt(mesh_path + ".tt3");

		BVH blas;
		std::vector<BuildObject> build_objects;
		for(uint i = 0; i < tt.size(); ++i)
			build_objects.push_back(tt.get_build_object(i));
		blas.build(build_objects);
		tt.reorder(build_objects);

		printf("Headers: %lld\n", tt.headers.size());
		printf("Vertices: %lld\n", tt.vertices.size());
		printf("Triangles: %lld\n", tt.triangles.size());
		printf("Nodes: %lld\n", tt.nodes.size() + blas.nodes.size());
		printf("Total Size: %lld bytes\n",
			tt.vertices.size() * sizeof(rtm::vec3) +
			tt.headers.size() * sizeof(TesselationTree3::Header) +
			tt.nodes.size() * sizeof(TesselationTree3::Node) +
			tt.triangles.size() * sizeof(CompactTri) +
			blas.nodes.size() * sizeof(BVH::Node)
		);

		global_data.tt3.blas = write_vector(main_memory, CACHE_BLOCK_SIZE, blas.nodes, heap_address);
		global_data.tt3.headers = write_vector(main_memory, CACHE_BLOCK_SIZE, tt.headers, heap_address);
		global_data.tt3.nodes = write_vector(main_memory, CACHE_BLOCK_SIZE, tt.nodes, heap_address);
		global_data.tt3.vertices = write_vector(main_memory, CACHE_BLOCK_SIZE, tt.vertices, heap_address);
		global_data.tt3.triangles = write_vector(main_memory, CACHE_BLOCK_SIZE, tt.triangles, heap_address);
	}
	else
	{
		BVH blas;
		std::vector<BuildObject> build_objects;
		for(uint i = 0; i < mesh.size(); ++i)
			build_objects.push_back(mesh.get_build_object(i));
		blas.build(build_objects);
		mesh.reorder(build_objects);

		printf("Vertices: %lld\n", mesh.vertices.size());
		printf("Faces: %lld\n", mesh.vertex_indices.size());
		printf("Nodes: %lld\n", blas.nodes.size());
		printf("Total Size: %lld bytes\n",
			mesh.vertices.size() * sizeof(rtm::vec3) +
			mesh.vertex_indices.size() * sizeof(rtm::uvec3) +
			blas.nodes.size() * sizeof(BVH::Node)
		);

		global_data.mesh.blas = write_vector(main_memory, CACHE_BLOCK_SIZE, blas.nodes, heap_address);
		global_data.mesh.vertex_indices = write_vector(main_memory, CACHE_BLOCK_SIZE, mesh.vertex_indices, heap_address);
		global_data.mesh.vertices = write_vector(main_memory, CACHE_BLOCK_SIZE, mesh.vertices, heap_address);
	}

	//we must preform mesh.reorder before copying the arrays
	global_data.ni = write_vector(main_memory, CACHE_BLOCK_SIZE, mesh.normal_indices, heap_address);
	global_data.normals = write_vector(main_memory, CACHE_BLOCK_SIZE, mesh.normals, heap_address);

	main_memory->direct_write(&global_data, sizeof(GlobalData), GLOBAL_DATA_ADDRESS);

	return global_data;
}


static void run_sim_describo(int argc, char* argv[])
{
	//simulate perameters
	std::string binary_path = argv[1];
	std::string mesh_path = argv[2];
	rtm::vec3 camera_vector;
	camera_vector.x = std::atof(argv[3]);
	camera_vector.y = std::atof(argv[4]);
	camera_vector.z = std::atof(argv[5]);
	uint camera_index = std::atoi(argv[6]);
	rtm::vec3 camera_pos = (1 << camera_index) * camera_vector;
	std::string profile_path = argv[7];
	std::string framebuffer_path = argv[8];

	//hardware spec
	uint mc_ports = 32;
	uint tp_port_size = 32;

	uint num_tps_per_tm = 16;
	uint num_tms_per_l2 = 64;
	uint num_l2 = 1;

	uint num_tps = num_tps_per_tm * num_tms_per_l2 * num_l2;
	uint num_tms = num_tms_per_l2 * num_l2;
	uint sfu_table_size = static_cast<uint>(ISA::RISCV::Type::NUM_TYPES);

	uint64_t mem_size = 4ull * 1024ull * 1024ull * 1024ull; //4GB

	//cached global data
	uint64_t stack_size = 2048 * 2; //2KB
	uint64_t global_data_size = 64 * 1024; //64KB for global data
	uint64_t binary_size = 64 * 1024; //64KB for executable data

	//glboal data
	vaddr_t mm_null_address = 0x0ull;
	vaddr_t mm_global_data_start = GLOBAL_DATA_ADDRESS;
	vaddr_t mm_binary_start = mm_global_data_start + global_data_size;

	vaddr_t mm_heap_start = mm_binary_start + binary_size;

	vaddr_t mm_stack_start = mem_size - stack_size;
	vaddr_t stack_pointer = mem_size;

	float l2_dynamic_read_energy = 0.240039e-9;
	float l2_bank_leakge_power = 30.6609e-3;

	float l1_dynamic_read_energy = 0.0464909e-9;
	float l1_bank_leakge_power = 3.11185e-3;

	//Custom instr. See defenitions above
	ISA::RISCV::isa[ISA::RISCV::CUSTOM_OPCODE0] = ISA::RISCV::DTRaX::custom0;

	Simulator simulator;

	std::vector<Units::UnitTP*> tps;
	std::vector<Units::UnitCache*> l1s;
	std::vector<Units::UnitCache*> l2s;
	std::vector<Units::UnitSFU*> sfus;

	std::vector<Units::UnitSFU*> sfu_tables(sfu_table_size* num_tms);

	Units::UnitDRAM mm(num_l2 * mc_ports, mem_size, &simulator); // mm.clear();
	simulator.register_unit(&mm);

	ELF elf(binary_path);
	vaddr_t global_pointer;
	paddr_t heap_address = mm.write_elf(elf);
	GlobalData global_data = initilize_buffers(binary_path, mesh_path, camera_pos, &mm, heap_address);

	Units::UnitTileScheduler tile_scheduler(num_tps, num_tms, global_data.framebuffer_width, global_data.framebuffer_height, 8, 8, &simulator);
	simulator.register_unit(&tile_scheduler);

	for(uint l2_index = 0; l2_index < num_l2; ++l2_index)
	{
		Units::UnitCache::Configuration l2_config;
		l2_config.size = 2 * 1024 * 1024;
		l2_config.associativity = 4;
		l2_config.penalty = 4;
		l2_config.num_ports = num_tms_per_l2;
		l2_config.num_banks = 32;
		l2_config.num_lfb = 4;
		l2_config.mem_map.add_unit(mm_null_address, &mm, l2_index * mc_ports, mc_ports);

		l2_config.dynamic_read_energy = 0.240039e-9;
		l2_config.bank_leakge_power = 30.6609e-3;

		l2s.push_back(new Units::UnitCache(l2_config));

		for(uint tm_i = 0; tm_i < num_tms_per_l2; ++tm_i)
		{
			simulator.start_new_unit_group();
			if(tm_i == 0) simulator.register_unit(l2s.back());

			uint tm_index = l2_index * num_tms_per_l2 + tm_i;

			Units::UnitCache::Configuration l1_config;
			l1_config.size = 16 * 1024;
			l1_config.associativity = 2;
			l1_config.penalty = 1;
			l1_config.num_ports = num_tps_per_tm;
			l1_config.port_size = tp_port_size;
			l1_config.num_banks = 4;
			l1_config.num_lfb = 4;
			l1_config.mem_map.add_unit(mm_null_address, l2s.back(), tm_i, 1);

			l1_config.dynamic_read_energy = 0.0464909e-9;
			l1_config.bank_leakge_power = 3.11185e-3;

			l1s.push_back(new Units::UnitCache(l1_config));
			simulator.register_unit(l1s.back());

			Units::UnitSFU** sfu_table = &sfu_tables[sfu_table_size * tm_index];

			//float piplines
			//sfus.push_back(_new Units::UnitSFU(16, 1, 2, num_tps_per_tm));
			//simulator.register_unit(sfus.back());
			//sfu_table[(uint)ISA::RISCV::Type::FADD] = sfus.back();
			//sfu_table[(uint)ISA::RISCV::Type::FMUL] = sfus.back();
			//sfu_table[(uint)ISA::RISCV::Type::FFMAD] = sfus.back();

			//int piplines
			//sfus.push_back(_new Units::UnitSFU(2, 1, 1, num_tps_per_tm));
			//simulator.register_unit(sfus.back());
			//sfu_table[(uint)ISA::RISCV::Type::IDIV] = sfus.back();

			//sfus
			//sfus.push_back(_new Units::UnitSFU(1, 1, 6, num_tps_per_tm));
			//simulator.register_unit(sfus.back());
			//sfu_table[(uint)ISA::RISCV::Type::FDIV] = sfus.back();
			
			//sfus.push_back(_new Units::UnitSFU(1, 1, 6, num_tps_per_tm));
			//simulator.register_unit(sfus.back());
			//sfu_table[(uint)ISA::RISCV::Type::FSQRT] = sfus.back();

			//sfus.push_back(_new Units::UnitSFU(1, 1, 2, num_tps_per_tm));
			//simulator.register_unit(sfus.back());
			//sfu_table[(uint)ISA::RISCV::Type::FRCP] = sfus.back();

			//intersectors
			//sfus.push_back(_new Units::UnitSFU(1, 1, 4, num_tps_per_tm));
			//simulator.register_unit(sfus.back());
			//sfu_table[(uint)ISA::RISCV::Type::BOXISECT] = sfus.back();
			
			// sfus.push_back(_new Units::UnitSFU(2, 18, 31, num_tps_per_tm));
			//simulator.register_unit(sfus.back());
			//sfu_table[(uint)ISA::RISCV::Type::TRIISECT] = sfus.back();

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
				tp_config.port_size = tp_port_size;
				tp_config.mem_map.add_unit(mm_null_address, &tile_scheduler, tm_index * num_tps_per_tm + tp_index, 1);
				tp_config.mem_map.add_unit(mm_global_data_start, l1s.back(), tp_index, 1);
				tp_config.mem_map.add_unit(mm_stack_start, nullptr, 0, 0);

				tps.push_back(new Units::UnitTP(tp_config));
				simulator.register_unit(tps.back());
				simulator.units_executing++;
			}
		}
	}

	auto start = std::chrono::high_resolution_clock::now();
	simulator.execute();
	auto stop = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

	Units::UnitTP::Log tp_log(0x10000);
	for(auto& tp : tps)
		tp_log.accumulate(tp->log);
	
	Units::UnitCache::Log l1_log;
	for(auto& l1 : l1s)
		l1_log.accumulate(l1->log);
	
	Units::UnitCache::Log l2_log;
	for(auto& l2 : l2s)
		l2_log.accumulate(l2->log);

	mm.print_usimm_stats(CACHE_BLOCK_SIZE, 4, simulator.current_cycle);

	printf("\nL2\n");
	l2_log.print_log();

	printf("\nL1\n");
	l1_log.print_log();

	printf("\nTP\n");
	tp_log.print_log();

	printf("\n\n");
	printf("#---------------------------------- Summary -----------------------------------#\n");
	printf("Benchmark: %s\n", binary_path.c_str());
	printf("Dataset: %s\n", mesh_path.c_str());
	printf("\n");
	
	printf("Simulation Runtime: %.2f s\n", duration.count() / 1000.0f);
	printf("\n");

	setlocale(LC_ALL, "");

	float frame_time = simulator.current_cycle / (2.0e9);
	printf("Cycles: %lld\n", simulator.current_cycle);
	printf("Frame Time @ 2GHz: %.2f ms\n", frame_time * 1000.0f);
	printf("Frame Rate @ 2GHz: %.0f fps\n", 1.0f / frame_time);
	printf("\n");

	printf("L1 Total: %lld\n", l1_log.get_total());
	printf("L2 Total: %lld\n", l2_log.get_total());
	printf("MM Total: %lld\n", l2_log._misses);
	printf("\n");

	//power in watts
	float l1_power = (l1_log.get_total_data_array_accesses() * l1_dynamic_read_energy) / frame_time + num_tms * l1_bank_leakge_power * 4;
	float l2_power = (l2_log.get_total_data_array_accesses() * l2_dynamic_read_energy) / frame_time + num_l2 * l2_bank_leakge_power * 32;
	float mm_power = mm.total_power_in_watts();
	float total_power = l1_power + l2_power + mm_power;

	float power_scale = 1.0f;
	printf("L1 Power: %.2f W\n", l1_power * power_scale);
	printf("L2 Power: %.2f W\n", l2_power * power_scale);
	printf("MM Power: %.2f W\n", mm_power * power_scale);
	printf("Total Power: %.2f W\n", total_power * power_scale);
	printf("\n");

	float l1_energy = l1_power * frame_time;
	float l2_energy = l2_power * frame_time;
	float mm_energy = mm_power * frame_time;
	float total_enregy = l1_energy + l2_energy + mm_energy;

	float energy_scale = 1.0e6f;
	printf("L1 Energy: %.0f uJ\n", l1_energy * energy_scale);
	printf("L2 Energy: %.0f uJ\n", l2_energy * energy_scale);
	printf("MM Energy: %.0f uJ\n", mm_energy * energy_scale);
	printf("Total Energy: %.0f uJ\n", total_enregy * energy_scale);
	printf("\n");


	FILE* prof_stream = fopen(profile_path.c_str(), "w");
	if(prof_stream)
	{
		tp_log.print_profile(mm._data_u8, prof_stream);
		fclose(prof_stream);
	}

	stbi_flip_vertically_on_write(true);
	paddr_t paddr_frame_buffer = reinterpret_cast<paddr_t>(global_data.framebuffer);
	mm.dump_as_png_uint8(paddr_frame_buffer, global_data.framebuffer_width, global_data.framebuffer_height, framebuffer_path);
}

}
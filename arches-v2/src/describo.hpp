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
InstructionInfo(0x0, "traxamoin", Type::TRAXAMOIN, Encoding::U, RegFile::INT, IMPL_DECL
{
	unit->mem_req.type = Units::MemoryRequest::Type::TRAXAMOIN;
	unit->mem_req.size = 4;

	unit->mem_req.dst.reg = instr.rd;
	unit->mem_req.dst.reg_file = 0;
	unit->mem_req.dst.sign_ext = false;

	unit->mem_req.vaddr = 0;
}),
InstructionInfo(0x1, "boxisect", Type::BOXISECT, Encoding::U, RegFile::FLOAT, IMPL_DECL
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

	unit->float_regs->registers[instr.rd].f32 = intersect(aabb, ray, inv_d);
}),
InstructionInfo(0x2, "triisect", Type::TRIISECT, Encoding::U, RegFile::INT, IMPL_DECL
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

const static InstructionInfo custom0(CUSTOM_OPCODE0, META_DECL{return isa_custom0[instr.r.funct3];});

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

static GlobalData initilize_buffers(std::string mesh_path, rtm::vec3 camera_pos, Units::UnitMainMemoryBase* main_memory, paddr_t& heap_address)
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

	if(mesh_path.back() == '2')
	{
		TesselationTree tt(mesh_path);

		BVH blas;
		std::vector<BuildObject> build_objects;
		for(uint i = 0; i < tt.size(); ++i)
			build_objects.push_back(tt.get_build_object(i));
		blas.build(build_objects);
		tt.reorder(build_objects);

		printf("Vertices: %lld\n", tt.vertices.size());
		printf("Headers: %lld\n", tt.headers.size());
		printf("Nodes: %lld\n", tt.nodes.size() + blas.nodes.size());
		printf("Total Size: %lld bytes\n",
			tt.vertices.size() * sizeof(rtm::vec3) +
			tt.headers.size() * sizeof(TesselationTree::Header) +
			tt.nodes.size() * sizeof(TesselationTree::Node) +
			blas.nodes.size() * sizeof(BVH::Node)
		);

		global_data.tt.blas = write_vector(main_memory, CACHE_LINE_SIZE, blas.nodes, heap_address);
		global_data.tt.headers = write_vector(main_memory, CACHE_LINE_SIZE, tt.headers, heap_address);
		global_data.tt.nodes = write_vector(main_memory, CACHE_LINE_SIZE, tt.nodes, heap_address);
		global_data.tt.vertices = write_vector(main_memory, CACHE_LINE_SIZE, tt.vertices, heap_address);
	}
	else
	{
		Mesh mesh(mesh_path);

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
	
		global_data.mesh.blas = write_vector(main_memory, CACHE_LINE_SIZE, blas.nodes, heap_address);
		global_data.mesh.vertex_indices = write_vector(main_memory, CACHE_LINE_SIZE, mesh.vertex_indices, heap_address);
		global_data.mesh.vertices = write_vector(main_memory, CACHE_LINE_SIZE, mesh.vertices, heap_address);
	}

	main_memory->direct_write(&global_data, sizeof(GlobalData), GLOBAL_DATA_ADDRESS);

	return global_data;
}


static void run_sim_describo(int argc, char* argv[])
{
	uint num_tps_per_tm = 16;
	uint num_tms_per_l2 = 64;
	uint num_l2 = 1;

	uint num_tps = num_tps_per_tm * num_tms_per_l2 * num_l2;
	uint num_tms = num_tms_per_l2 * num_l2;
	uint num_sfus = static_cast<uint>(ISA::RISCV::Type::NUM_TYPES) * num_tms;
	
	//Custom instr. See defenitions above
	ISA::RISCV::isa[ISA::RISCV::CUSTOM_OPCODE0] = ISA::RISCV::DTRaX::custom0;

	Units::UnitCache::Configuration l1_config;
	l1_config.associativity = 2;
	l1_config.size = 16 * 1024;
	l1_config.block_size = CACHE_LINE_SIZE;
	l1_config.num_banks = 4;
	l1_config.num_ports = num_tps_per_tm;
	l1_config.penalty = 1;
	l1_config.num_mshr = 4;

	//from cacti (22nm)
	l1_config.dynamic_read_energy = 0.0464909e-9;
	l1_config.bank_leakge_power = 3.11185e-3;

	Units::UnitCache::Configuration l2_config;
	l2_config.associativity = 4;
	l2_config.size = 2 * 1024 * 1024;
	l2_config.block_size = CACHE_LINE_SIZE;
	l2_config.num_banks = 32;
	l2_config.num_ports = num_tms_per_l2;
	l2_config.penalty = 4;
	l2_config.num_mshr = 4;
	
	//from cacti (22nm)
	l2_config.dynamic_read_energy = 0.240039e-9;
	l2_config.bank_leakge_power = 30.6609e-3;


	std::string binary_path = argv[1];
	std::string mesh_path = argv[2];

	printf("\n");

	rtm::vec3 camera_vector;
	camera_vector.x = std::atof(argv[3]);
	camera_vector.y = std::atof(argv[4]);
	camera_vector.z = std::atof(argv[5]);
	uint camera_index = std::atoi(argv[6]);
	rtm::vec3 camera_pos = (1 << camera_index) * camera_vector;

	std::string profile_path = argv[7];
	std::string framebuffer_path = argv[8];

	Simulator simulator;
	ELF elf(binary_path);

	Units::UnitDRAM mm(l2_config.num_banks * num_l2, 1024ull * 1024ull * 1024ull, &simulator); mm.clear();
	Units::UnitAtomicRegfile amoin(num_tps_per_tm * num_tms_per_l2 * num_l2, &simulator);
	paddr_t heap_address = mm.write_elf(elf);	GlobalData global_data = initilize_buffers(mesh_path, camera_pos, &mm, heap_address);
	Units::UnitTileScheduler tile_scheduler(num_tps, num_tms, global_data.framebuffer_width, global_data.framebuffer_height, 8, 8, &simulator);

	simulator.register_unit(&mm);
	simulator.register_unit(&tile_scheduler);

	//std::vector<Units::UnitCoreSimple*> cores;
	std::vector<Units::UnitTP> tps; tps.reserve(num_tps);
	std::vector<Units::UnitCache> l1s; l1s.reserve(num_tms);
	std::vector<Units::UnitCache> l2s; l2s.reserve(num_l2);

	std::vector<Units::UnitSFU> sfus; sfus.reserve(num_tms * (uint)ISA::RISCV::Type::NUM_TYPES);
	std::vector<std::vector<Units::UnitSFU*>> sfu_tables;
	std::vector<Units::MemoryUnitMap> mu_maps;
	mu_maps.reserve(num_tps);

	Units::UnitTP::Configuration tp_config;
	tp_config.backing_memory = mm._data_u8;
	tp_config.pc = elf.elf_header->e_entry.u64;

	size_t  stack_size = 2 * 1024;
	paddr_t stack_top = 1024ull * 1024ull * 1024ull;
	paddr_t stack_start = stack_top - num_tps * stack_size;

	for(uint k = 0; k < num_l2; ++k)
	{
		uint mm_port = k;
		l2s.emplace_back(l2_config, &mm, mm_port, &simulator);
		Units::UnitCache* l2 = &l2s.back();

		for(uint j = 0; j < num_tms_per_l2; ++j)
		{
			simulator.start_new_unit_group();

			if(j == 0) simulator.register_unit(l2);

			uint l2_port = j;
			l1s.emplace_back(l1_config, l2, l2_port, &simulator);
			Units::UnitCache* l1 = &l1s.back();

			simulator.register_unit(l1);

			sfu_tables.emplace_back(static_cast<uint>(ISA::RISCV::Type::NUM_TYPES), nullptr);
			Units::UnitSFU** sfu_table = sfu_tables.back().data();

		#if 1
			sfus.emplace_back(8, 1, 2, num_tps_per_tm, &simulator);
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FADD)] = &sfus.back();
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FSUB)] = &sfus.back();
			simulator.register_unit(&sfus.back());

			sfus.emplace_back(8, 1, 2, num_tps_per_tm, &simulator);
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FMUL)] = &sfus.back();
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FFMAD)] = &sfus.back();
			simulator.register_unit(&sfus.back());

			sfus.emplace_back(2, 1, 1, num_tps_per_tm, &simulator);
			sfu_table[static_cast<uint>(ISA::RISCV::Type::IMUL)] = &sfus.back();
			sfu_table[static_cast<uint>(ISA::RISCV::Type::IDIV)] = &sfus.back();
			simulator.register_unit(&sfus.back());

			sfus.emplace_back(1, 1, 6, num_tps_per_tm, &simulator);
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FDIV)] = &sfus.back();
			simulator.register_unit(&sfus.back());

			sfus.emplace_back(1, 1, 6, num_tps_per_tm, &simulator);
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FSQRT)] = &sfus.back();
			simulator.register_unit(&sfus.back());

			sfus.emplace_back(1, 1, 2, num_tps_per_tm, &simulator);
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FRCP)] = &sfus.back();
			simulator.register_unit(&sfus.back());

			sfus.emplace_back(1, 1, 4, num_tps_per_tm, &simulator);
			sfu_table[static_cast<uint>(ISA::RISCV::Type::BOXISECT)] = &sfus.back();
			simulator.register_unit(&sfus.back());

			sfus.emplace_back(2, 18, 31, num_tps_per_tm, &simulator);
			sfu_table[static_cast<uint>(ISA::RISCV::Type::TRIISECT)] = &sfus.back();
			simulator.register_unit(&sfus.back());

		#else
			sfus.emplace_back(8, 1, 4, num_tps_per_tm, &simulator);
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FADD)] = &sfus.back();
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FSUB)] = &sfus.back();
			simulator.register_unit(&sfus.back());

			sfus.emplace_back(8, 1, 4, num_tps_per_tm, &simulator);
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FMUL)] = &sfus.back();
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FFMAD)] = &sfus.back();
			simulator.register_unit(&sfus.back());

			sfus.emplace_back(2, 1, 5, num_tps_per_tm, &simulator);
			sfu_table[static_cast<uint>(ISA::RISCV::Type::IMUL)] = &sfus.back();
			sfu_table[static_cast<uint>(ISA::RISCV::Type::IDIV)] = &sfus.back();
			simulator.register_unit(&sfus.back());

			sfus.emplace_back(1, 3, 11, num_tps_per_tm, &simulator);
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FDIV)] = &sfus.back();
			simulator.register_unit(&sfus.back());

			sfus.emplace_back(1, 3, 12, num_tps_per_tm, &simulator);
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FSQRT)] = &sfus.back();
			simulator.register_unit(&sfus.back());

			sfus.emplace_back(1, 1, 4, num_tps_per_tm, &simulator);
			sfu_table[static_cast<uint>(ISA::RISCV::Type::FRCP)] = &sfus.back();
			simulator.register_unit(&sfus.back());

			sfus.emplace_back(1, 1, 8, num_tps_per_tm, &simulator);
			sfu_table[static_cast<uint>(ISA::RISCV::Type::CUSTOM1)] = &sfus.back();
			simulator.register_unit(&sfus.back());

			sfus.emplace_back(2, 18, 31, num_tps_per_tm, &simulator);
			sfu_table[static_cast<uint>(ISA::RISCV::Type::CUSTOM2)] = &sfus.back();
			simulator.register_unit(&sfus.back());
		#endif

			mu_maps.emplace_back();
			Units::MemoryUnitMap* mu_map = &mu_maps.back();

			mu_maps.back().add_unit(0ull, &tile_scheduler, (k * num_tms_per_l2 + j) * num_tps_per_tm);
			mu_maps.back().add_unit(GLOBAL_DATA_ADDRESS, l1, 0);
			mu_maps.back().add_unit(stack_start, nullptr, 0);

			tp_config.sfu_table = sfu_table;
			tp_config.mem_map = mu_map;

			tp_config.tm_index = (k * num_tms_per_l2 + j);

			for(uint i = 0; i < num_tps_per_tm; ++i)
			{
				tp_config.tp_index = i;
				tp_config.sp = stack_top; stack_top -= stack_size;

				tps.emplace_back(tp_config, &simulator);
				simulator.register_unit(&tps.back());
			}
		}
	}

	auto start = std::chrono::high_resolution_clock::now();
	simulator.execute();
	auto stop = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

	Units::UnitTP::Log tp_log(0x10000);
	for(auto& tp : tps)
		tp_log.accumulate(tp.log);
	
	Units::UnitCache::Log l1_log;
	for(auto& l1 : l1s)
		l1_log.accumulate(l1.log);
	
	Units::UnitCache::Log l2_log;
	for(auto& l2 : l2s)
		l2_log.accumulate(l2.log);

	mm.print_usimm_stats(l2_config.block_size, 4, simulator.current_cycle);

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
	float l1_power = (l1_log.get_total() * l1_config.dynamic_read_energy) / frame_time + num_tms * l1_config.bank_leakge_power * l1_config.num_banks;
	float l2_power = (l2_log.get_total() * l2_config.dynamic_read_energy) / frame_time + num_l2 * l2_config.bank_leakge_power * l2_config.num_banks;
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
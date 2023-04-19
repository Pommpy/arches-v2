#pragma once
#include "stdafx.hpp"

#include "mesh.hpp"
#include "tesselation-tree.hpp"

class TesselationTree3
{
public:
	struct Header
	{
		rtm::uvec3 vi;
		float max_db;

		uint last_lod;
		uint root_node_offset;
		uint root_triangle_offset;
		uint id_offset;
	};

	struct Node
	{
		AABB     aabb;
		uint32_t dbs;
		uint32_t _pad;
	};

#ifdef ARCH_X86
	std::vector<Header> headers;
	std::vector<Node> nodes;
	std::vector<rtm::vec3> vertices;
	std::vector<CompactTri> triangles;
	std::vector<float> sah_costs;

	TesselationTree3(std::string file_path)
	{
		printf("Loading: %s\r", file_path.c_str());

		std::ifstream stream(file_path, std::ios::binary);
		if(!stream.is_open()) return;

		_deserialize_vector(headers, stream);
		_deserialize_vector(nodes, stream);
		_deserialize_vector(vertices, stream);
		_deserialize_vector(triangles, stream);
		_deserialize_vector(sah_costs, stream);

		stream.close();

		printf("Loaded: %s \n", file_path.c_str());
	};

	uint size() { return (uint)headers.size(); };

	void reorder(std::vector<BuildObject>& ordered_build_objects)
	{
		std::vector<Header> temp_headers(headers);
		for(uint i = 0; i < ordered_build_objects.size(); ++i)
			headers[i] = temp_headers[ordered_build_objects[i].index];
	};

	BuildObject get_build_object(uint index)
	{
		BuildObject object;
		object.aabb = nodes[headers[index].root_node_offset].aabb;
		object.cost = sah_costs[index];
		object.index = index;
		return object;
	};
#endif
};

struct TesselationTree3Pointers
{
	BVH::Node* blas;
	TesselationTree3::Header* headers;
	TesselationTree3::Node* nodes;
	glm::vec3* vertices;
	CompactTri* triangles;
};
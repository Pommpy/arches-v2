#pragma once
#include "stdafx.hpp"

#include "bvh.hpp"
#include "mesh.hpp"

struct alignas(32) TreeletNode
{
	AABB aabb;
	union
	{
		uint32_t data;
		struct
		{
			uint32_t is_leaf         : 1;
			uint32_t is_treelet_leaf : 1;
			uint32_t last_tri_offset : 3;
			uint32_t child_index     : 27;
		};
	};
};

struct TreeletTriangle
{
	Triangle tri;
	uint     id;
};

struct alignas(8 * 1024) Treelet
{
	uint32_t _words[16 * 1024];
};

#ifdef ARCH_X86
class TreeletBVH
{
public:
	std::vector<Treelet> treelets;

	TreeletBVH(const BVH& bvh, const Mesh& mesh)
	{
		printf("Treelet BVH Building\n");
		build_treelet_breadth_first(bvh, mesh);
		printf("Treelet BVH Built\n");
	}

	uint get_node_size(uint node, const BVH& bvh, const Mesh& mesh)
	{
		uint node_size = sizeof(TreeletNode);
		if(bvh.nodes[node].is_leaf)
			node_size += sizeof(TreeletTriangle) * (bvh.nodes[node].lst_chld_ofst + 1);

		return node_size;
	}

	void build_treelet_breadth_first(const BVH& bvh, const Mesh& mesh)
	{
		std::vector<std::vector<uint>> treelet_assignments;

		std::queue<uint> root_node_queue;
		root_node_queue.push(bvh.nodes[0].fst_chld_ind);
		
		size_t bytes = 0;

		while(!root_node_queue.empty())
		{
			treelet_assignments.push_back({});

			std::queue<uint> node_queue;
			node_queue.push(root_node_queue.front()); root_node_queue.pop();

			uint bytes_left = sizeof(Treelet);
			while(!node_queue.empty())
			{
				uint node = node_queue.front(); node_queue.pop();
				uint node_size = get_node_size(node + 0, bvh, mesh) + get_node_size(node + 1, bvh, mesh);

				if(node_size < bytes_left)
				{
					bytes_left -= node_size;

					treelet_assignments.back().push_back(node + 0);
					treelet_assignments.back().push_back(node + 1);

					if(!bvh.nodes[node + 0].is_leaf) node_queue.push(bvh.nodes[node + 0].fst_chld_ind);
					if(!bvh.nodes[node + 1].is_leaf) node_queue.push(bvh.nodes[node + 1].fst_chld_ind);
				}
				else root_node_queue.push(node);
			}
			bytes += sizeof(Treelet) - bytes_left;
		}

		treelets.resize(treelet_assignments.size());

		std::unordered_map<uint, std::pair<uint, uint>> node_map;

		uint num_nodes_assigned = 0;
		uint num_tris_assigned = 0;

		for(uint i = treelets.size() - 1; i != ~0u; --i)
		{
			uint num_nodes = treelet_assignments[i].size();
			num_nodes_assigned += num_nodes;

			for(uint j = 0; j < num_nodes; ++j)
			{
				uint node_id = treelet_assignments[i][j];
				BVHNode node = bvh.nodes[node_id];
				node_map[node_id].first = i;
				node_map[node_id].second = j * (sizeof(TreeletNode) / 4);
			}

			uint current_word = num_nodes * (sizeof(TreeletNode) / 4);
			
			for(uint j = 0; j < num_nodes; ++j)
			{
				uint node_id = treelet_assignments[i][j];
				BVHNode node = bvh.nodes[node_id];

				TreeletNode& tnode = ((TreeletNode*)treelets[i]._words)[j];
				tnode.aabb = node.aabb;
				tnode.is_leaf = node.is_leaf;

				if(node.is_leaf)
				{
					tnode.last_tri_offset = node.lst_chld_ofst;
					tnode.child_index = current_word;

					TreeletTriangle* tris = (TreeletTriangle*)(&treelets[i]._words[current_word]);
					for(uint k = 0; k <= node.lst_chld_ofst; ++k)
					{
						num_tris_assigned++;
						tris[k].id = node.fst_chld_ind + k;
						tris[k].tri = mesh.triangles[node.fst_chld_ind + k];
					}

					current_word += (sizeof(TreeletTriangle) / 4) * (node.lst_chld_ofst + 1);
				}
				else
				{
					tnode.is_treelet_leaf = node_map[node.fst_chld_ind].first != i;
					if(tnode.is_treelet_leaf) tnode.child_index = node_map[node.fst_chld_ind].first;
					else                      tnode.child_index = node_map[node.fst_chld_ind].second;

				}
			}
		}

		printf("Nodes Assigned: %u/%u\n", num_nodes_assigned, bvh.num_nodes);
		printf("Tris Assigned: %u/%u\n", num_tris_assigned, mesh.num_triangles);
		printf("Treelets: %lu\n", treelets.size());
		printf("Bytes/Treelet: %lu\n", bytes / treelets.size());
	}

};
#endif

#pragma once
#include "stdafx.hpp"

#include "triangle.hpp"
#include "node.hpp"
#include "hit.hpp"
#include "ray.hpp"

#if 1
bool inline intersect(const Node* nodes, const Triangle* triangles, const Ray& ray, bool an_hit, Hit& hit)
{
	rtm::vec3 inv_d = rtm::vec3(1.0f) / ray.d;

	constexpr uint root_index = 0;
	constexpr uint stack_size = 32;

	struct NodeStackEntry
	{
		float hit_t {T_MAX};
		union
		{
			uint32_t data;
			struct
			{
				uint32_t is_leaf : 1;
				uint32_t lst_chld_ofst : 3;
				uint32_t fst_chld_ind : 28;
			};
		};
	};

	float root_hit_t = nodes[root_index].aabb.intersect(ray, inv_d);
	if(root_hit_t >= hit.t) return false;

	uint node_stack_index = ~0u; NodeStackEntry node_stack[stack_size];
	node_stack[++node_stack_index] = {root_index, nodes[root_index].data};

	bool is_hit = false;
	while(node_stack_index != ~0u)
	{
		NodeStackEntry current_entry = node_stack[node_stack_index--];

	TRAV:
		if(current_entry.hit_t >= hit.t) continue;
		if(!current_entry.is_leaf)
		{
			Node nodes_local[2] = {nodes[current_entry.fst_chld_ind + 0u], nodes[current_entry.fst_chld_ind + 1u]};

			float hit_ts[2];
			hit_ts[0] = nodes_local[0].aabb.intersect(ray, inv_d);
			hit_ts[1] = nodes_local[1].aabb.intersect(ray, inv_d);

			if(hit_ts[0] < hit_ts[1])
			{
				
				if(hit_ts[1] < hit.t) node_stack[++node_stack_index] = {hit_ts[1], nodes_local[1].data};
				if(hit_ts[0] < hit.t)
				{
					current_entry = {hit_ts[0], nodes_local[0].data}; 
					goto TRAV;
				}
			}
			else
			{
				if(hit_ts[0] < hit.t) node_stack[++node_stack_index] = {hit_ts[0], nodes_local[0].data};
				if(hit_ts[1] < hit.t)
				{
					current_entry = {hit_ts[1], nodes_local[1].data}; 
					goto TRAV;
				}
			}
		}
		else
		{
			if(an_hit)
			{
				uint id = current_entry.fst_chld_ind;
				for(uint i = 0; i <= current_entry.lst_chld_ofst; ++i)
				{
					Triangle t = triangles[id];
					if(t.intersect(ray, hit))
					{
						hit.prim_id = id;
						return true;
					}
					id++;
				}
			}
			else
			{
				uint id = current_entry.fst_chld_ind;
				for(uint i = 0; i <= current_entry.lst_chld_ofst; ++i)
				{
					Triangle t = triangles[id];
					if(t.intersect(ray, hit))
					{
						hit.prim_id = id;
						is_hit = true;
					}
					id++;
				}
			}
		}
	}
	return is_hit;
}
#else
bool inline intersect(const Node* nodes, const Triangle* triangles, const Ray& ray, bool an_hit, Hit& hit)
{
	constexpr uint root_index = 0;
	constexpr uint stack_size = 32;

	struct NodeStackEntry
	{
		uint node_index {0};
		float hit_t     {T_MAX};
	};

	float root_hit_t = nodes[root_index].aabb.intersect(ray);
	if(root_hit_t >= hit.t) return false;

	uint node_stack_index = ~0u; NodeStackEntry node_stack[stack_size];
	node_stack[++node_stack_index] = {root_index, root_hit_t};

	bool is_hit = false;
	while(node_stack_index != ~0u)
	{
		NodeStackEntry& current_entry = node_stack[node_stack_index--];
		const Node& current_node = nodes[current_entry.node_index];
		if(!current_node.is_leaf)
		{
			NodeStackEntry entrys[2] = {{current_node.fst_chld_ind + 0u, nodes[current_node.fst_chld_ind + 0u].aabb.intersect(ray)},
		                                {current_node.fst_chld_ind + 1u, nodes[current_node.fst_chld_ind + 1u].aabb.intersect(ray)}};

			if(entrys[0].hit_t < entrys[1].hit_t)
			{
				if(entrys[1].hit_t < hit.t) node_stack[++node_stack_index] = entrys[1];
				if(entrys[0].hit_t < hit.t) node_stack[++node_stack_index] = entrys[0];
			}
			else
			{
				if(entrys[0].hit_t < hit.t) node_stack[++node_stack_index] = entrys[0];
				if(entrys[1].hit_t < hit.t) node_stack[++node_stack_index] = entrys[1];
			}
		}
		else
		{
			for(uint i = 0; i <= current_node.lst_chld_ofst; ++i)
			{
				uint obj_ind = current_node.fst_chld_ind + i;
				if(triangles[obj_ind].intersect(ray, hit))
				{
					if(an_hit) return true;
					else     is_hit = true;
				}
			}
		}
	}
	return is_hit;
}
#endif

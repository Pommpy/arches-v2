#pragma once
#include "stdafx.hpp"

#include "aabb.hpp"
#include "triangle.hpp"
#include "mesh.hpp"
#include "node.hpp"

#ifdef ARCH_X86
class BVH
{
public:
	Node* nodes{nullptr};
	size_t num_nodes{0};

	//BUILD
	struct BuildObject
	{
		AABB  aabb  {};
		float cost  {1.0f};
		uint  index {0u};
	};

	class _BuildObjectComparatorSpacial
	{
	public:
		_BuildObjectComparatorSpacial(uint axis) : _axis(axis) {}
		bool operator()(const BuildObject& a, const BuildObject& b) { return a.aabb.get_centroid()[_axis] < b.aabb.get_centroid()[_axis]; };
	private:
		uint _axis;
	};

	struct _BuildEvent
	{
		BuildObject* build_objects{nullptr};
		uint num_build_objects{0};
		uint absolute_offset{0};
		bool is_leaf{false};

		uint split_build_objects(AABB parent_aabb)
		{
			uint best_spliting_index = 0;
			uint best_spliting_axis = parent_aabb.get_longest_axis();
			float best_spliting_cost = FLT_MAX;

			for(uint spliting_axis = 0; spliting_axis < 3; ++spliting_axis)
			{
				_BuildObjectComparatorSpacial comparator(spliting_axis);
				std::sort(build_objects, build_objects + num_build_objects, comparator);

				std::vector<float> cost_left = {0.0f};
				{
					AABB aabb;
					float object_cost_sum = 0.0f;
					for(uint i = 0; i < num_build_objects; ++i)
					{
						object_cost_sum += build_objects[i].cost;
						aabb.add_aabb(build_objects[i].aabb);
						float cost = object_cost_sum * aabb.surface_area() / parent_aabb.surface_area();
						cost_left.push_back(cost);
					}
				}

				std::vector<float> cost_right = {0.0f};
				{
					AABB aabb;
					float object_cost_sum = 0.0f;
					for(uint i = num_build_objects - 1u; i != ~0u; --i)
					{
						object_cost_sum += build_objects[i].cost;
						aabb.add_aabb(build_objects[i].aabb);
						float cost = object_cost_sum * aabb.surface_area() / parent_aabb.surface_area();
						cost_right.push_back(cost);
					}
					std::reverse(cost_right.begin(), cost_right.end());
				}

				for(uint i = 0; i < cost_left.size(); ++i)
				{
					float cost = parent_aabb.get_cost() + cost_left[i] + cost_right[i];
					if(cost < best_spliting_cost)
					{
						best_spliting_index = i;
						best_spliting_axis = spliting_axis;
						best_spliting_cost = cost;
					}
				}
			}

			_BuildObjectComparatorSpacial comparator(best_spliting_axis);
			std::sort(build_objects, build_objects + num_build_objects, comparator);

			//second case shouldnt happen?
			if(best_spliting_index == 0 || best_spliting_index == num_build_objects)
			{
				if(num_build_objects <= 8) return ~0;
				else                       return num_build_objects / 2;
			}

			return best_spliting_index;
		}
	};

	BVH(Mesh& mesh)
	{
		std::cout << "Building BVH4\n";

		_BuildEvent build_event;
		build_event.num_build_objects = mesh.num_triangles;
		std::vector<BuildObject> build_objects(build_event.num_build_objects);
		build_event.build_objects = build_objects.data();

		for(uint i = 0; i < mesh.num_triangles; ++i)
		{
			build_event.build_objects[i].aabb = mesh.triangles[i].get_aabb();
			build_event.build_objects[i].cost = mesh.triangles[i].get_cost();
			build_event.build_objects[i].index = i;
		}

		std::vector<Node>_nodes; _nodes.resize(1);
		std::cout << "BVH Cost: " << (_build(build_event, _nodes, 0) + _nodes[0].aabb.get_cost()) << "\n";

		num_nodes = _nodes.size();
		nodes = new Node[num_nodes];
		for(uint i = 0; i < num_nodes; ++i)
			nodes[i] = _nodes[i];

		std::vector<Triangle> temp_tris(mesh.num_triangles);
		for(uint i = 0; i < mesh.num_triangles; ++i)
			temp_tris[i] = mesh.triangles[i];

		for(uint i = 0; i < build_objects.size(); ++i)
			mesh.triangles[i] = temp_tris[build_objects[i].index];

		std::cout << "BVH4 Built\n";
	}

	virtual ~BVH()
	{
		if(nodes) delete[] nodes;
	}

private:
	float _build(_BuildEvent& build_event, std::vector<Node>& _nodes, uint node_index)
	{
		AABB aabb;
		for(uint i = 0; i < build_event.num_build_objects; ++i)
			aabb.add_aabb(build_event.build_objects[i].aabb);

		std::vector<_BuildEvent> build_events = {build_event};
		for(uint i = 0; i < 1; ++i)
		{
			std::vector<_BuildEvent> new_build_events;
			for(auto& build_event : build_events)
			{
				uint splitting_index = build_event.split_build_objects(aabb);
				if(splitting_index != ~0)
				{
					{
						_BuildEvent new_event;
						new_event.build_objects = build_event.build_objects;
						new_event.num_build_objects = splitting_index;
						new_event.absolute_offset = build_event.absolute_offset;
						new_build_events.push_back(new_event);
					}
					{
						BuildObject* build_objects1 = build_event.build_objects + splitting_index;
						uint num_build_objects1 = build_event.num_build_objects - splitting_index;
						uint absolute_offset1 = build_event.absolute_offset + splitting_index;
						new_build_events.push_back({build_objects1, num_build_objects1, absolute_offset1, false});
					}
				}
				else
				{
					new_build_events.push_back(build_event);
				}
			}
			if(new_build_events.size() == build_events.size()) break;
			build_events.clear();
			build_events = new_build_events;
		}

		//didn't do any splitting meaning this build event can become a leaf node
		if(build_events.size() == 1) return _build_leaf(build_event, _nodes, node_index);
		else
		{
			_nodes[node_index].aabb = aabb;
			_nodes[node_index].is_leaf = 0;
			_nodes[node_index].lst_chld_ofst = build_events.size() - 1;
			_nodes[node_index].fst_chld_ind = _nodes.size();

			float cost = aabb.get_cost();
			for(uint i = 0; i < build_events.size(); ++i) _nodes.push_back({});
			for(uint i = 0; i < build_events.size(); ++i)
			{
				uint child_index = _nodes[node_index].fst_chld_ind + i;
				float temp_cost;
				if(build_events[i].is_leaf)	temp_cost = _build_leaf(build_events[i], _nodes, child_index);
				else                        temp_cost = _build(build_events[i], _nodes, child_index);
				cost += temp_cost * _nodes[child_index].aabb.surface_area() / aabb.surface_area();
			}
			return cost;
		}
	}

	float _build_leaf(_BuildEvent& build_event, std::vector<Node>& _nodes, uint node_index)
	{
		assert(build_event.num_build_objects <= 8 && build_event.num_build_objects >= 1);

		AABB aabb;
		float cost = 0.0f;
		for(uint i = 0; i < build_event.num_build_objects; ++i)
		{
			aabb.add_aabb(build_event.build_objects[i].aabb);
			cost += build_event.build_objects[i].cost;
		}

		_nodes[node_index].aabb = aabb;
		_nodes[node_index].is_leaf = 1;
		_nodes[node_index].lst_chld_ofst = build_event.num_build_objects - 1;
		_nodes[node_index].fst_chld_ind = build_event.absolute_offset;

		return cost;
	}
};
#endif

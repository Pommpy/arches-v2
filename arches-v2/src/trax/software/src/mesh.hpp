#pragma once
#include "stdafx.hpp"

#include "triangle.hpp"
#include "rtm.hpp"

#ifdef ARCH_X86
class Mesh
{
public:
	Triangle* triangles{nullptr};
	uint      num_triangles{0};

	Mesh(const char* file_path)
	{
		std::vector<rtm::uvec3> vertex_indices;
		std::vector<rtm::uvec3> normal_indices;
		std::vector<rtm::uvec3> tex_coord_indices;

		std::vector<rtm::vec3> vertices;
		std::vector<rtm::vec3> normals;
		std::vector<rtm::vec2> tex_coords;

		bool has_v = false;
		bool has_n = false;
		bool has_t = false;

		std::ifstream file(file_path);
		if (file.is_open())
		{
			std::string line;
			while (std::getline(file, line))
			{
				std::istringstream lineStream(line);
				std::string type;
				lineStream >> type;

				if (type == "v")
				{
					has_v = true;
					float v0, v1, v2;
					lineStream >> v0 >> v1 >> v2;
					vertices.push_back(rtm::vec3(v0, v1, v2));
				}
				else if (type == "vn")
				{
					has_n = true;
					float n0, n1, n2;
					lineStream >> n0 >> n1 >> n2;
					normals.push_back(rtm::vec3(n0, n1, n2));
				}
				else if (type == "vt")
				{
					has_t = true;
					float t0, t1;
					lineStream >> t0 >> t1;
					tex_coords.push_back(rtm::vec2(t0, t1));
				}
				else if (type == "f")
				{
					vertex_indices.push_back({});
					if (has_t) tex_coord_indices.push_back({});
					if (has_n) normal_indices.push_back({});

					char c; uint ib;
					for (uint i = 0; i < 3; ++i)
					{
						lineStream >> ib;
						vertex_indices.back()[i] = ib - 1;
						if (has_t)
						{
							lineStream >> c;
							lineStream >> ib;
							tex_coord_indices.back()[i] = ib - 1;
							if (has_n)
							{
								lineStream >> c;
								lineStream >> ib;
								normal_indices.back()[i] = ib - 1;
							}
						}
						else if (has_n)
						{
							lineStream >> c >> c;
							lineStream >> ib;
							normal_indices.back()[i] = ib - 1;
						}
					}
				}
			}
		}
		else
		{
			std::cout << "Failed To Load: " << file_path <<  "\n";
			return;
		}

		if (!has_t)
		{
			tex_coords.push_back(rtm::vec2(0.0f));

			for (size_t i = 0; i < vertex_indices.size(); ++i)
				tex_coord_indices.push_back(rtm::uvec3(0));
		}

		if (!has_n)
		{
			for (size_t i = 0; i < vertices.size(); ++i)
				normals.push_back(rtm::vec3(0.0f));

			for (size_t i = 0; i < vertex_indices.size(); ++i)
				normal_indices.push_back(vertex_indices[i]);

			for (size_t i = 0; i < vertex_indices.size(); ++i)
			{
				rtm::vec3 edge1 = vertices[vertex_indices[i][1]] - vertices[vertex_indices[i][0]];
				rtm::vec3 edge2 = vertices[vertex_indices[i][2]] - vertices[vertex_indices[i][0]];

				rtm::vec3 normal = rtm::normalize(rtm::cross(edge1, edge2));

				for (uint j = 0; j < 3; ++j)
					normals[vertex_indices[i][j]] += normal;
			}

			for (size_t i = 0; i < vertices.size(); ++i)
				normals[i] = rtm::normalize(normals[i]);
		}

		num_triangles = vertex_indices.size();
		triangles = new Triangle[num_triangles];
		for(uint i = 0; i < num_triangles; ++i)
		{
			triangles[i].vrts[0] = vertices[vertex_indices[i][0]];
			triangles[i].vrts[1] = vertices[vertex_indices[i][1]];
			triangles[i].vrts[2] = vertices[vertex_indices[i][2]];
			//for(uint j = 0; j < 3; ++j)
			//	for(uint k = 0; k < 3; ++k)
			//		std::cout << triangles[i].vrts[j][k] << ", ";
			//std::cout << "\n";
		}

		std::cout << "Loaded: " << file_path << ", " << has_v << ", " << has_t << ", " << has_n << "\n";
	}

	virtual ~Mesh()
	{
		if(triangles) delete[] triangles;
	}
};
#endif

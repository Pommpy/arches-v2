#pragma once
#include "stdafx.hpp"

struct CompactTri
{
	uint64_t data[4];
};


#ifdef ARCH_X86
template <typename T>
inline T _deserialize(std::ifstream& stream)
{
	T value;
	stream.read((char*)&value, sizeof(T));
	return value;
}

template <typename T>
inline void _deserialize_vector(std::vector<T>& vector, std::ifstream& stream)
{
	size_t size = _deserialize<size_t>(stream);
	vector.resize(size);
	stream.read((char*)vector.data(), vector.size() * sizeof(T));
}
#endif
#pragma once
#include "../../stdafx.hpp"

#include "../simulator/simulator.hpp"
#include "../util/round-robin-arbitrator.hpp"

namespace Arches { namespace Units {

template<typename T>
class ConnectionGroup
{
private:
	struct alignas(64) _64Aligned
	{
		uint8_t data[64];
	};

	uint8_t* _pending;
	T*       _data;
	uint     _size;

public:
	ConnectionGroup(uint size) : _size(size)
	{
		_size = size;
		_pending = (uint8_t*)(_new _64Aligned[(size + 63) / 64]);
		_data = (T*)(_new _64Aligned[(size * sizeof(T) + 63) / 64]);
	
		for(uint i = 0; i < size; ++i)
			_pending[i] = 0u;
	}

	~ConnectionGroup()
	{
		delete[] (uint8_t*)_pending;
		delete[] (uint8_t*)_data;
	}
	
	size_t size() const { return _size; }

	bool get_pending(size_t index) const 
	{ 
		assert(index < _size);
		return _pending[index];
	}
	void set_pending(size_t index)
	{
		assert(index < _size);
		assert(_pending[index] == 0);
		_pending[index] = 1;
	}
	void clear_pending(size_t index)
	{
		assert(index < _size);
		assert(_pending[index] == 1);
		_pending[index] = 0;
	}

	const T& get_data(size_t index) const 
	{
		assert(index < _size);
		return _data[index];
	}
	void set_data(const T& data, size_t index)
	{
		assert(index < _size);
		assert(_pending[index] == 0);
		_data[index] = data;
	}
};

//avoid false sharing by aligning to cache line
class alignas(64) UnitBase
{
public:
	bool executing{false};

	UnitBase(Simulator* simulator) {}

	virtual void clock_rise() = 0;
	virtual void clock_fall() = 0;
};

}}
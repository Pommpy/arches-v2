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

	//TODO add arbitrator directly to the calss
	//Should be much more efficent since each line has one byte and they are alligne they will likely be on the same line and byte compare with zero is super cheap if they are l1 hits
	T*       _data;
	uint8_t* _pending;
	uint     _size;
	uint     _arb_index;

public:

	ConnectionGroup(uint size) : _size(size)
	{
		_arb_index = 0;
		_size = size;
		_pending = (uint8_t*)(_new _64Aligned[(size + 63) / 64]);
		_data = (T*)(_new _64Aligned[(size * sizeof(T) + 63) / 64]);
	
		for(uint i = 0; i < size; ++i)
			_pending[i] = 0u;
	}

	~ConnectionGroup()
	{
		delete[] (_64Aligned*)_pending;
		delete[] (_64Aligned*)_data;
	}
	
	size_t size() const { return _size; }
	
	//return true if a request was added since the last time this function was called. This might be a decent optimization
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
	uint get_next_pending()
	{
		uint index = _arb_index;
		do
		{
			if(_pending[index])
			{
				_arb_index = index + 1;
				if(_arb_index == _size) _arb_index = 0;
				return index;
			}

			if(++index == _size) index = 0;
		} while(index != _arb_index);

		return ~0u;
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
class UnitBase
{
public:
	Simulator* simulator{nullptr};
	virtual void clock_rise() = 0;
	virtual void clock_fall() = 0;
};

}}
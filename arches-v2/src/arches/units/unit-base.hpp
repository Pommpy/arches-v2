#pragma once
#include "../../stdafx.hpp"

#include "../simulator/simulator.hpp"

namespace Arches { namespace Units {

template<typename T>
class ConnectionGroup
{
private:
	struct alignas(64) _64Aligned
	{
		uint8_t data[64];
	};

	//Should be much more efficent since each line has one byte and they are alligne they will likely be on the same line
	//reads to the data only happen if pending flag is set and ack is done by clearing the pending bit
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
	
	bool transfer_pending(size_t index) const 
	{ 
		assert(index < _size);
		return _pending[index];
	}
	uint get_next()
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
	void acknowlege(size_t index)
	{
		assert(index < _size);
		assert(_pending[index]);
		_pending[index] = 0;
	}
	const T& transfer(size_t index) const
	{
		assert(index < _size);
		assert(_pending[index]);
		return _data[index]; 
	}
	void add_transfer(const T& data, size_t index)
	{
		assert(index < _size);
		assert(!_pending[index]);
		_data[index] = data;
		_pending[index] = 1;
	}
};

class UnitBase
{
public:
	Simulator* simulator{nullptr};
	uint32_t unit_id{~0u};
	virtual void clock_rise() = 0;
	virtual void clock_fall() = 0;
};

}}
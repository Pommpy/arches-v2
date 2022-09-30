#pragma once
#include "../../stdafx.hpp"

#include "../util/round-robin-arbitrator.hpp"
#include "../simulator/simulator.hpp"
//#include "../simulator/input-buffer.hpp"
//#include "../simulator/output-buffer.hpp"
//#include "../simulator/acknowledge-buffer.hpp"

namespace Arches { namespace Units {

union Pending
{
	__m512i _data_aligner;
	bool data[64];
};

#if 0
template<typename T>
class BusGroup
{
public:


	struct BusForward
	{
		BusGroup<T>* bus{nullptr};
		size_t       index{0};
	};

protected:
	std::vector<Pending>    pending;
	std::vector<BusForward> bus_forwards;
	std::vector<T>          data;

public:
	BusGroup(uint size)
	{
		pending.resize((size + 63) / 64);
		for(uint i = 0; i < pending.size(); ++i)
			for(uint j = 0; j < pending.size(); ++j)
				pending[i].data[j] = false;

		bus_forwards.resize(size);
		data.resize(size);
	}

	static void link_busses(size_t index0, BusGroup<T>* bus0, size_t index1, BusGroup<T>* bus1)
	{
		bus0->bus_forwards[index0].bus = bus1;
		bus0->bus_forwards[index0].index = index1;
	}

	static void unlink_busses(size_t index0, BusGroup<T>* bus0, size_t index1, BusGroup<T>* bus1)
	{
		bus0->bus_forwards[index0].bus = nullptr;
		bus1->bus_forwards[index1].bus = nullptr;
	}

	size_t size() const { return data.size();}

	bool get_pending(size_t index) { return pending.data()->data[index]; }
	void set_pending(size_t index)
	{ 
		assert(pending.data()->data[index] == false);
		pending.data()->data[index] = true;
		if(bus_forwards[index].bus) bus_forwards[index].bus->set_pending(bus_forwards[index].index);
	}
	void clear_pending(size_t index)
	{
		assert(pending.data()->data[index] == true);
		pending.data()->data[index] = false;
		if(bus_forwards[index].bus) bus_forwards[index].bus->clear_pending(bus_forwards[index].index);
	}

	const T& get_bus_data(size_t index) { return data[index]; }
	void set_bus_data(const T& data, size_t index) 
	{
		this->data[index] = data;
		if(bus_forwards[index].bus) bus_forwards[index].bus->set_bus_data(data, bus_forwards[index].index);
	}
};
#else
template<typename T>
class BusGroup
{
public:


	struct BusForward
	{
		BusGroup<T>* bus{nullptr};
		size_t       index{0};
	};

protected:
	std::vector<Pending>    pending;
	std::vector<T>          data;

public:
	BusGroup(uint size)
	{
		pending.resize((size + 63) / 64);
		for(uint i = 0; i < pending.size(); ++i)
			for(uint j = 0; j < pending.size(); ++j)
				pending[i].data[j] = false;

		data.resize(size);
	}
	size_t size() const { return data.size(); }

	bool get_pending(size_t index) { return pending.data()->data[index]; }
	void set_pending(size_t index)
	{
		assert(pending.data()->data[index] == false);
		pending.data()->data[index] = true;
	}
	void clear_pending(size_t index)
	{
		assert(pending.data()->data[index] == true);
		pending.data()->data[index] = false;
	}

	const T& get_bus_data(size_t index) { return data[index]; }
	void set_bus_data(const T& data, size_t index)
	{
		assert(pending.data()->data[index] == false);
		this->data[index] = data;
	}
};
#endif


class UnitBase
{
public:
	bool executing{false};

	UnitBase(Simulator* simulator)
	{
		simulator->register_unit(this);
	}

	virtual void clock_rise() = 0;
	virtual void clock_fall() = 0;
};

}}
#include "simulator.hpp"

#include "../units/unit-base.hpp"


#if 1
namespace Arches {

void Simulator::register_unit(Units::UnitBase * unit)
{
	_units.push_back(unit);
	_unit_groups.back().end++;
	unit->simulator = this;
}

void Simulator::start_new_unit_group()
{
	_unit_groups.emplace_back(static_cast<uint>(_units.size()), static_cast<uint>(_units.size()));
}


#ifndef _DEBUG
#define USE_TBB
#endif

#ifdef USE_TBB
//tbb controled block ranges
//#define UNIT_LOOP tbb::parallel_for(tbb::blocked_range<uint>(0, _units.size()), [&](tbb::blocked_range<uint> r) { for(uint i = r.begin(); i < r.end(); ++i) {
//#define UNIT_LOOP_END }});
// 
//custom block ranges
#define UNIT_LOOP tbb::parallel_for(tbb::blocked_range<uint>(0, _unit_groups.size(), 1), [&](tbb::blocked_range<uint> r) { for(uint i = _unit_groups[r.begin()].start; i < _unit_groups[r.begin()].end; ++i) {
#define UNIT_LOOP_END }});
#else
#define UNIT_LOOP for(uint i = 0; i < _units.size(); ++i) {
#define UNIT_LOOP_END }
#endif

void Simulator::_clock_rise()
{
	UNIT_LOOP
		_units[i]->clock_rise();
	UNIT_LOOP_END
}

void Simulator::_clock_fall()
{
	UNIT_LOOP
		_units[i]->clock_fall();
	UNIT_LOOP_END
}

void Simulator::execute()
{
	while(units_executing > 0)
	{
		_clock_rise();
		_clock_fall();
		current_cycle++;
	}
}

}
#else
namespace Arches
{
	
	Simulator::Simulator(uint num_threads) : _thread_pool(num_threads, nullptr), _barrier(_thread_pool.size())
	{ 
		_unit_groups.emplace_back(0u, 0u); 
	}

	void Simulator::register_unit(Units::UnitBase* unit)
	{
		_units.push_back(unit);
		_unit_groups.back().size++;
	}

	void Simulator::start_new_unit_group()
	{
		_unit_groups.emplace_back(static_cast<uint>(_units.size()), 0u);
	}


#if 0
	void Simulator::_clock_rise()
	{
		uint g = _rise_index.fetch_add(1);
		while(g < _unit_groups.size())
		{
			UnitGroup unit_group = _unit_groups[g];
			for(uint i = unit_group.start; i < (unit_group.start + unit_group.size); ++i)
			{
				_units[i]->clock_rise();
			}
			g = _rise_index.fetch_add(1);
		}
	}

	void Simulator::_clock_fall()
	{
		bool done = true;

		uint g = _fall_index.fetch_add(1);
		while(g < _unit_groups.size())
		{
			UnitGroup unit_group = _unit_groups[g];
			for(uint i = unit_group.start; i < (unit_group.start + unit_group.size); ++i)
			{
				_units[i]->clock_fall();
				_done = _done && !_units[i]->executing;
			}
			g = _fall_index.fetch_add(1);
		}

		if(!done) _done = false;
	}
#else
	void Simulator::_clock_rise()
	{
		uint i = _rise_index.fetch_add(1);
		while(i < _units.size())
		{
			_units[i]->clock_rise();
			i = _rise_index.fetch_add(1);
		}
	}

	void Simulator::_clock_fall()
	{
		uint i = _fall_index.fetch_add(1); bool done = true;
		while(i < _units.size())
		{
			_units[i]->clock_fall();
			_done = _done && !_units[i]->executing;
			i = _fall_index.fetch_add(1);
		}
		if(!done) _done = false;
	}
#endif

	void Simulator::execute()
	{
		_done = false;

		for(uint i = 0; i < _thread_pool.size(); ++i)
			_thread_pool[i] = _new std::thread(&Simulator::_thread_work, this, i);

		for(uint i = 0; i < _thread_pool.size(); ++i)
		{
			_thread_pool[i]->join();
			delete _thread_pool[i];
			_thread_pool[i] = nullptr;
		}
	}

	void Simulator::_thread_work(uint thread_id)
	{
		SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

		while(!_done)
		{
			_barrier.arrive_and_wait();

			if(thread_id == 0)
			{
				_done = true;
				_fall_index = 0;
			}
			_clock_rise();

			_barrier.arrive_and_wait();

			if(thread_id == 0)
			{
				_rise_index = 0;
			}
			_clock_fall();

			_barrier.arrive_and_wait();
			
			if(thread_id == 0)
			{
				current_cycle++;
			}
		}
	}

}
#endif
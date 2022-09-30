#include "simulator.hpp"

#include "../units/unit-base.hpp"

namespace Arches {

void Simulator::register_unit(Units::UnitBase * unit)
{
	_units.push_back(unit);
	_unit_groups.back().size++;
}

void Simulator::start_new_unit_group()
{
	_unit_groups.emplace_back(static_cast<uint>(_units.size()), 0u);
}


#ifndef _DEBUG
#define USE_TBB
#endif

#ifdef USE_TBB
#define UNIT_LOOP tbb::parallel_for(tbb::blocked_range<uint>(0, _units.size()), [&](tbb::blocked_range<uint> r) { for(uint i = r.begin(); i < r.end(); ++i) {
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
		if(_units[i]->executing) _done = false;
	UNIT_LOOP_END
}

void Simulator::execute()
{
	_done = false;
	while(!_done)
	{
		_done = true;
		_clock_rise();
		_clock_fall();
		current_cycle++;
	}
}

}
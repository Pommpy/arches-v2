#pragma once
#include "../../stdafx.hpp"

#include "../util/barrier.hpp"
//#include "input-buffer.hpp"

namespace Arches {

namespace Units
{
	class UnitBase;
}

class Simulator
{
private:
	struct UnitGroup
	{
		uint start;
		uint size;

		UnitGroup() = default;
		UnitGroup(uint start, uint size) : start(start), size(size) {}
	};

	std::vector<UnitGroup> _unit_groups;
	std::vector<Units::UnitBase*> _units;
	std::atomic_bool              _done{true};

public:
	cycles_t current_cycle{0};

	Simulator() { _unit_groups.emplace_back(0u, 0u); }

	void register_unit(Units::UnitBase* unit);
	void start_new_unit_group();

	void _clock_rise();
	void _clock_fall();

	void execute();
};

}
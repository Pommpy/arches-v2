#pragma once
#include "../../stdafx.hpp"

#include "../util/barrier.hpp"
#include "input-buffer.hpp"

namespace Arches {

namespace Units
{
	class UnitBase;
}

class Simulator
{
private:
	std::vector<InputBufferBase*> _input_buffers;
	std::vector<Units::UnitBase*> _units;
	std::atomic_bool              _done{true};

public:
	cycles_t current_cycle{0};

	Simulator() {}

	void register_input_buffer(InputBufferBase* input_buffer);
	void register_unit(Units::UnitBase* unit);

	void _update_buffers();
	void _send_acknowledgements();
	void _execute_cycle();

	void execute();
};

}
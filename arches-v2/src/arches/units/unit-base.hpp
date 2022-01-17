#pragma once
#include "../../stdafx.hpp"

#include "../simulator/simulator.hpp"
#include "../simulator/input-buffer.hpp"
#include "../simulator/output-buffer.hpp"

namespace Arches { namespace Units {

class UnitBase
{
public:
	bool                          executing{false};
	OutputBuffer                  output_buffer;

	UnitBase(Simulator* simulator)
	{
		simulator->register_unit(this);
	}

	virtual void execute() = 0;
};

}}
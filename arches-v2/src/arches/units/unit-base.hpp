#pragma once
#include "../../stdafx.hpp"

#include "../simulator/simulator.hpp"
#include "../simulator/input-buffer.hpp"
#include "../simulator/output-buffer.hpp"
#include "../simulator/acknowledge-buffer.hpp"

namespace Arches { namespace Units {

class UnitBase
{
public:
	bool                          executing{false};
	OutputBuffer                  output_buffer;
	AcknowledgeBuffer             acknowledge_buffer;

	UnitBase(Simulator* simulator) : output_buffer(this), acknowledge_buffer()
	{
		simulator->register_unit(this);
	}

	virtual void execute() = 0;
	virtual void acknowledge(buffer_id_t buffer_id) {};
};

}}
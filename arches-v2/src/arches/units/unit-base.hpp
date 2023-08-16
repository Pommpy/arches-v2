#pragma once
#include "../../stdafx.hpp"
#include "../simulator/simulator.hpp"



namespace Arches { namespace Units {

	
/*! \class UnitBase
	\brief Abstract class declaring foundation layout for units.

	This class can provides interface to be implemented by simulator units. Each unit has to define its behavior on clock-rise 
	and clock-fall signal.
*/
class UnitBase
{
public:
	//< Member reference to parent simulator
	Simulator* simulator{nullptr};
	//< Member 64 bit i
	uint64_t   unit_id{~0ull};

	virtual void clock_rise() = 0;	//< Clock rise interface
	virtual void clock_fall() = 0;	//< Clock fall interface
};

}}
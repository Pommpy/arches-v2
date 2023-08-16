/*! \file simulator.h 
*	\brief Describes generic simulator class
*/

#pragma once
#include "../../stdafx.hpp"


#if 1
namespace Arches {

namespace Units
{
	class UnitBase;
}

/*! \class Simulator
	\brief Generic simulator template.

	This class can be further specialized to provide specific implementations.
*/
class Simulator
{
private:
	/*! \struct UnitGroup
	* 
	*/
	struct UnitGroup
	{
		uint start;
		uint end;

		UnitGroup() = default;
		UnitGroup(uint start, uint end) : start(start), end(end) {}
	};

	//! Collection of unit groups.
	std::vector<UnitGroup> _unit_groups; 
	
	//! Collection of unit bases. 
	std::vector<Units::UnitBase*> _units;
	
public:
	
	//! Thread safe unit execution counter
	std::atomic_uint units_executing{0};
	
	//! 64 bit cycle counter
	cycles_t current_cycle{ 0 };
	
	//! Default Constructor
	Simulator() { _unit_groups.emplace_back(0u, 0u); }

	//! A function to register unit to be executed by this simulator
	void register_unit(Units::UnitBase* unit);

	//! A function to clear and recreate unit base collection
	void start_new_unit_group();

	//! A function to define routines executed by simulator when it receives clock rise signal
	void _clock_rise();

	//! A function to define routines executed by simulator when it receives clock fall signal
	void _clock_fall();

	//! A function to define all the execution routines
	void execute();
};

}
#else

#include <thread>
#include <barrier>

namespace Arches
{

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
		std::atomic_bool              _done{false};

		std::atomic_uint _rise_index{0};
		std::atomic_uint _fall_index{0};

		std::vector<std::thread*> _thread_pool;
		std::barrier<std::_No_completion_function> _barrier;

	public:
		cycles_t current_cycle{~0ull};

		Simulator(uint num_threads);

		void register_unit(Units::UnitBase* unit);
		void start_new_unit_group();

		void _clock_rise();
		void _clock_fall();

		void _thread_work(uint thread_id);

		void execute();

	};

}
#endif
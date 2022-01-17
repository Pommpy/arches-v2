#include "simulator.hpp"

#include "../units/unit-base.hpp"

namespace Arches {

void Simulator::register_input_buffer(InputBufferBase* input_buffer)
{
	//add the input buffer and set its index so other untis know where to find it
	//this means that sending a message to a unit only requires you to querry the input port
	input_buffer->id = _input_buffers.size();
	_input_buffers.push_back(input_buffer);
}

void Simulator::register_unit(Units::UnitBase * unit)
{
	_units.push_back(unit);
}

#ifndef _DEBUG
#define USE_TBB
#endif

#ifdef USE_TBB
#define UNIT_LOOP tbb::parallel_for(tbb::blocked_range<uint>(0, _units.size(), 4), [&](tbb::blocked_range<uint> r) { for(uint i = r.begin(); i < r.end(); ++i) {
#define UNIT_LOOP_END }});
#else
#define UNIT_LOOP for(uint i = 0; i < _units.size(); ++i) {
#define UNIT_LOOP_END }
#endif

void Simulator::_update_buffers()
{
	UNIT_LOOP
		void* data; uint32_t size, dst_buffer, dst_port;
		while((data = _units[i]->output_buffer.pop_message(size, dst_buffer, dst_port)) != nullptr)
			_input_buffers[dst_buffer]->write_data(dst_port, data, size);
	UNIT_LOOP_END
}

void Simulator::_execute_cycle()
{
	UNIT_LOOP
		_units[i]->execute();
		if(_units[i]->executing) _done = false;
	UNIT_LOOP_END
}

void Simulator::execute()
{
	_done = false;
	while(!_done)
	{
		_done = true;
		_update_buffers();
		_execute_cycle();
		++current_cycle;
		if((current_cycle % (1024)) == 0)
			std::cout << current_cycle << "\n";
	}
}

}
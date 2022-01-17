#pragma once

#include "../../stdafx.hpp"

#include "../simulator/simulator.hpp"

namespace Arches {

	class InputBufferBase
	{
	public:
		uint32_t id;
		virtual void write_data(uint index, void* data, uint size) = 0;
	};

	template <typename T>
	class InputBuffer : public InputBufferBase
	{
		std::vector<bool> valid;
		std::vector<T>    data;

		uint32_t last_port{0};
		uint32_t current_port{0};

		bool looped{false};

	public:
		InputBuffer(uint32_t size) : valid(size, false), data(size)
		{

		}

		void write_data(uint port, void* input_data, uint size) override
		{
			assert(size == sizeof(T));
			assert(!valid[port]); //check that this isn't in use
			data[port] = *reinterpret_cast<T*>(input_data);
			valid[port] = true;
		}

		uint size() { return valid.size(); }

		//round robin get_next returns nullptr 
		T* get_next()
		{
			uint dummy;
			return get_next(dummy);
		}

		T* get_next(uint& index)
		{
			do 
			{
				if(looped) return nullptr;
				current_port = (current_port + 1) % valid.size();
				if(current_port == last_port) looped = true;
			}
			while(!valid[current_port]);

			index = current_port;
			return &data[current_port];
		}

		T* get(uint index)
		{
			return &data[index];
		}

		//round robin get_next returns nullptr 
		void clear_current()
		{
			valid[current_port] = false;
		}

		//round robin get_next returns nullptr 
		void clear_index(uint index)
		{
			valid[index] = false;
		}

		void rest_arbitrator_round_robin()
		{
			last_port = current_port;
			looped = false;
		}

		void rest_arbitrator_lowest_index()
		{
			last_port = current_port = 0;
			looped = false;
		}
	};

}
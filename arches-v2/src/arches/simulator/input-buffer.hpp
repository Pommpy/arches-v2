#pragma once

#include "../../stdafx.hpp"

#include "../simulator/simulator.hpp"

namespace Arches {
	namespace Units {
		class UnitBase;
	}

	class InputBufferBase
	{
	public:
		buffer_id_t id {~0u};
		virtual void write_data(uint index, Units::UnitBase* sending_unit, void* data, uint size) = 0;
	};

	template <typename T>
	class InputBuffer : public InputBufferBase
	{
		std::vector<bool>             valid;
		std::vector<Units::UnitBase*> sending_unit;
		std::vector<T>                data;

		uint32_t last_port{0};
		uint32_t current_port{0};

		bool looped{false};

	public:

		InputBuffer(uint32_t size) : valid(size, false), sending_unit(size), data(size)
		{
		}

		uint size() { return static_cast<uint>(valid.size()); }

		void resize(uint size)
		{
			valid.resize(size, false);
			sending_unit.resize(size, nullptr);
			data.resize(size);
		}

		void write_data(uint port, Units::UnitBase* sending_unit, void* input_data, uint size) override
		{
			assert(size == sizeof(T));
			assert(!valid[port]); //check that this isn't in use
			this->data[port] = *reinterpret_cast<T*>(input_data);
			this->sending_unit[port] = sending_unit;
			this->valid[port] = true;
		}

		//round robin get_next returns nullptr 

		uint get_next_index()
		{
			do
			{
				if(looped) return ~0;
				current_port = (current_port + 1) % valid.size();
				if(current_port == last_port) looped = true;
			} while(!valid[current_port]);

			return current_port;
		}

		/*
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
		*/

		T* get_message(uint index)
		{
			return &data[index];
		}

		Units::UnitBase* get_sending_unit(uint index)
		{
			return sending_unit[index];
		}

		//round robin get_next returns nullptr 
		void clear(uint index)
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
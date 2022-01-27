#pragma once

#include "../../stdafx.hpp"

namespace Arches {

	namespace Units {
		class UnitBase;
	}

	class AcknowledgeBuffer
	{
		uint32_t num_entries{0};

		struct BufferEntry
		{
			Units::UnitBase* unit;
			buffer_id_t buffer;
		};

		std::vector<BufferEntry> entries;

	public:
		AcknowledgeBuffer() {}

		void resize(uint32_t max_messages)
		{
			entries.resize(max_messages);
		}

		~AcknowledgeBuffer() {}

		void push_message(Units::UnitBase* unit, buffer_id_t buffer)
		{
			assert(num_entries < entries.size());
			entries[num_entries].buffer = buffer;
			entries[num_entries].unit = unit;
			num_entries++;
		}

		bool pop_message(Units::UnitBase*& unit, buffer_id_t& buffer)
		{
			if(num_entries > 0)
			{
				BufferEntry& entry = entries[--num_entries];
				unit = entry.unit;
				buffer = entry.buffer;
				return true;
			}
			return false;
		}
	};

}
#pragma once

#include "../../stdafx.hpp"

namespace Arches {

	class OutputBuffer
	{
		uint32_t num_entries;
		uint32_t data_index;

		struct BufferEntry
		{
			uint32_t dst_buffer;
			uint32_t dst_port;
			uint32_t num_bytes;
			uint32_t data_index;
		};

		std::vector<BufferEntry> entries;
		uint8_t*                 data_u8{nullptr};

	public:
		OutputBuffer()
		{
			num_entries = 0;
			data_index = 0;
		}

		void resize(uint32_t max_messages, uint32_t max_message_size)
		{
			entries.resize(max_messages);
			if(data_u8) delete[] data_u8;
			data_u8 = reinterpret_cast<uint8_t*>(_new uint64_t[max_messages * (max_message_size + 7) / 8]);
		}

		~OutputBuffer() { delete[] data_u8; }

		template<typename T>
		void push_message(T* message, uint32_t buffer_id, uint32_t port)
		{
			assert(num_entries < entries.size());
			entries[num_entries].dst_buffer = buffer_id;
			entries[num_entries].dst_port = port;
			entries[num_entries].num_bytes = sizeof(T);
			entries[num_entries].data_index = data_index;
			std::memcpy(data_u8 + data_index, message, sizeof(T));
			data_index += sizeof(T); 
			num_entries++;
		}

		void* pop_message(uint32_t& size_out, uint32_t& buffer_id_out, uint32_t& port_out)
		{
			if(num_entries != 0)
			{
				BufferEntry& entry = entries[--num_entries];
				size_out = entry.num_bytes;
				buffer_id_out = entry.dst_buffer;
				port_out = entry.dst_port;

				data_index -= size_out;
				return (data_u8 + entry.data_index);
			}
			data_index = 0;
			return nullptr;
		}
	};

}
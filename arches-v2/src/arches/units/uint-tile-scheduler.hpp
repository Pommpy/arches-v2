#pragma once 
#include "../../stdafx.hpp"

#include "unit-base.hpp"
#include "unit-memory-base.hpp"
#include "../util/bit-manipulation.hpp"

namespace Arches { namespace Units {

class UnitTileScheduler : public UnitMemoryBase
{
public:
	UnitTileScheduler(uint num_tp, uint num_tm, uint frame_width, uint frame_height, uint tile_width, uint tile_height,  Simulator* simulator) :
		frame_width(frame_width), frame_height(frame_height), tile_width(tile_width), tile_height(tile_height), UnitMemoryBase(num_tp, simulator),
		tp_per_tm(num_tp / num_tm), bank_states(num_tm, tp_per_tm)
	{
		executing = false;

		num_tiles_width = frame_width / tile_width;
		num_tiles_height = frame_height / tile_height;
		num_tiles = num_tiles_width * num_tiles_height;

		tile_size = tile_width * tile_height;

		tm_shift = log2i(tp_per_tm);
	}

	uint num_requests{0};

	uint frame_width;
	uint frame_height;

	uint num_tiles_width;
	uint num_tiles_height;
	uint num_tiles;

	uint tile_width;
	uint tile_height;
	uint tile_size;

	uint tm_shift;
	uint tp_per_tm;

	uint next_tile{0};
	struct Bank
	{
		RoundRobinArbitrator arbitrator;

		MemoryRequest request_item;
		uint request_index{~0u};

		uint tile_index{~0u};
		uint tile_fill{~0u};

		Bank(uint clients) : arbitrator(clients)
		{

		}
	};

	std::vector<Bank> bank_states;

	void clock_rise() override
	{
		for(uint i = 0; i < bank_states.size(); ++i)
		{
			for(uint j = 0; j < tp_per_tm; ++j)
			{
				if(request_bus.get_pending(i * tp_per_tm + j)) 
					bank_states[i].arbitrator.push_request(j);
			}

			if((bank_states[i].request_index = bank_states[i].arbitrator.pop_request()) != ~0)
			{
				bank_states[i].request_index += i * tp_per_tm;
				bank_states[i].request_item = request_bus.get_data(bank_states[i].request_index);
				request_bus.clear_pending(bank_states[i].request_index);
			}
		}
	}

	void clock_fall() override
	{
		for(uint i = 0; i < bank_states.size(); ++i)
		{
			if(bank_states[i].request_index != ~0)
			{
				Bank& bank = bank_states[i];

			#if 0
				if(num_requests++ < 1) bank.request_item.data_u32 = 577 + (1023 - 667) * 1024;
				else                   bank.request_item.data_u32 = ~0u;
			#else
				if(bank.tile_fill >= tile_size)
				{
					bank.tile_index = next_tile++;
					bank.tile_fill = 0;
				}

				if(bank.tile_index < num_tiles)
				{
					uint tile_index = bank.tile_index;
					uint sub_tile_index = bank.tile_fill++;

					uint tile_x = tile_index % num_tiles_width;
					uint tile_y = tile_index / num_tiles_width;

					uint x = (tile_x * tile_width) + sub_tile_index % tile_width;
					uint y = (tile_y * tile_height) + sub_tile_index / tile_width;

					uint index = x + y * frame_width;
					bank.request_item.data_u32 = index; //todo value
				}
				else
				{
					bank.request_item.data_u32 = ~0u;
				}

				num_requests++;
				if((num_requests % 4096) == 0) 
					printf(" pixels rendered: %d\r", num_requests);
			#endif

				//AMO_RETURNS carry data unlike normal load returns
				bank.request_item.type = MemoryRequest::Type::AMO_RETURN;
				return_bus.set_data(bank.request_item, bank.request_index);
				return_bus.set_pending(bank.request_index);
			}
		}
	}
};

}}
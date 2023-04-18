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
		frame_width(frame_width), frame_height(frame_height), tile_width(tile_width), tile_height(tile_height), UnitMemoryBase(num_tp),
		tp_per_tm(num_tp / num_tm), bank_states(num_tm, tp_per_tm)
	{
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

		uint32_t return_reg;

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
				if(request_bus.transfer_pending(i * tp_per_tm + j)) 
					bank_states[i].arbitrator.add(j);
			}

			if((bank_states[i].request_index = bank_states[i].arbitrator.next()) != ~0u)
			{
				bank_states[i].arbitrator.remove(bank_states[i].request_index);

				bank_states[i].request_index += i * tp_per_tm;
				bank_states[i].request_item = request_bus.transfer(bank_states[i].request_index);
				request_bus.acknowlege(bank_states[i].request_index);
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

					bank.return_reg = index;
					bank.request_item.data = &bank.return_reg;
				}
				else
				{
					bank.return_reg = ~0u;
					bank.request_item.data = &bank.return_reg;
				}

				num_requests++;
				printf(" pixels rendered: %d\r", num_requests);

				bank.request_item.type = MemoryRequest::Type::LOAD_RETURN;
				return_bus.add_transfer(bank.request_item, bank.request_index);
			}
		}
	}
};

}}
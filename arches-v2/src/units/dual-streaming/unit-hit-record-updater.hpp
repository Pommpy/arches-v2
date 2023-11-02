#pragma once

#include "../unit-base.hpp"
#include "../../simulator/transactions.hpp"
#include "../../simulator/interconnects.hpp"
#include "../unit-dram.hpp"



namespace Arches { namespace Units { namespace DualStreaming {

/*!
* \brief The hit record updater receives hit records from TMs and update the record in the DRAM
* 
* The hit record updater is divided into multiple channels (eg. number of channels in DRAM). It uses a casscadeCrossBar to process requests.
* Each channel maintains a local hit record cache. When a new hit record is requesting data from DRAM, we use the cache to store the record. When the cache is full, the TPs are blocked.
* Each channel processes one request from TPs in one cycle. Channels issue write request first (if available), then read request.
*/


struct HitInfo {
	rtm::Hit hit;
	paddr_t hit_address;
};

struct HitRecordUpdaterRequest
{
	enum TYPE{
		STORE = 0,
		LOAD
	};
	TYPE type; //load or conditional store
	HitInfo hit_info;
	uint port;
};

class UnitHitRecordUpdater : public UnitBase {
public:
	struct Configuration
	{
		paddr_t hit_record_start;

		uint cache_size;
		uint associativity;

		uint num_tms;

		UnitMainMemoryBase* main_mem;
		uint                main_mem_port_offset{ 0 };
		uint                main_mem_port_stride{ 1 };
	};


	/*
	* Hit record cache in the updater. It is a set-associative cache used for storing requesting updates.
	*/
	class HitRecordCache {
	public:
		enum class State : uint8_t
		{
			ALREADY_CLOEST, // Get data from DRAM and wait to write
			LOAD_FROM_DRAM,
			EMPTY,
		};
	private:
		struct CacheElement {
			State state;
			HitInfo hit_info;
			uint lru = 0;
		};
		std::vector<CacheElement> cache;
		uint cache_size; // How many hit records
		uint associativity;
		uint num_set;
	public:
		HitRecordCache(uint _cache_size, uint _associativity): cache_size(_cache_size), associativity(_associativity), num_set(_cache_size / _associativity){
			cache.resize(cache_size, { State::EMPTY });
		}
		
		uint look_up(HitInfo hit_info) {
			uint set_id = hit_info.hit_address % num_set;
			uint start_index = set_id * associativity, end_index = start_index + associativity;
			assert(end_index <= cache_size);
			uint found_index = ~0;
			uint counter = 0;
			for (int i = start_index; i < end_index; i++) {
				if (cache[i].state != State::EMPTY && cache[i].hit_info.hit_address == hit_info.hit_address) {
					found_index = i;
					counter += 1;
				}
			}
			assert(counter < 2);
			if (!counter) return ~0;
			for (int i = start_index; i < end_index; i++) {
				if (cache[i].lru < cache[found_index].lru) cache[i].lru++;
			}
			cache[found_index].lru = 0;
			return found_index;
		}

		void replace(HitInfo hit_info) {
			uint set_id = hit_info.hit_address % num_set;
			uint start_index = set_id * associativity, end_index = start_index + associativity;
			assert(end_index <= cache_size);
			uint found_index = ~0;
			for (int i = start_index; i < end_index; i++) {
				if (cache[i].state != State::EMPTY && cache[i].hit_info.hit_address == hit_info.hit_address) {
					found_index = i;
					if (hit_info.hit.t < cache[i].hit_info.hit.t) {
						cache[i].hit_info = hit_info;
					}
				}
			}
			assert(found_index != ~0);
		}

		/*!
		* \brief Hit record from DRAM.
		*/
		uint process_dram_hit(HitInfo hit_info) {
			uint set_id = hit_info.hit_address % num_set;
			uint start_index = set_id * associativity, end_index = start_index + associativity;
			assert(end_index <= cache_size);
			uint found_index = ~0;
			for (int i = start_index; i < end_index; i++) {
				if (cache[i].state == State::LOAD_FROM_DRAM && cache[i].hit_info.hit_address == hit_info.hit_address) {
					found_index = i;
					cache[i].state = State::ALREADY_CLOEST;
					if (cache[i].hit_info.hit.t < hit_info.hit.t) { // The record in cache is closer
						cache[i].hit_info = hit_info;
					}
				}
			}
			assert(found_index != ~0);
			return found_index;
		}

		uint insert(HitInfo hit_info) {
			uint set_id = hit_info.hit_address % num_set;
			uint start_index = set_id * associativity, end_index = start_index + associativity;
			assert(end_index <= cache_size);
			uint found_index = ~0;
			for (int i = start_index; i < end_index; i++) {
				if (cache[i].state == State::EMPTY) {
					found_index = i;
					break;
				}
			}
			if (found_index == ~0) {
				uint lru = 0;
				for (int i = start_index; i < end_index; i++) {
					if (cache[i].state == State::ALREADY_CLOEST && cache[i].lru >= lru) {
						found_index = i;
						lru = cache[i].lru;
					}
				}
			}
			if (found_index != ~0) {
				cache[found_index].state = State::LOAD_FROM_DRAM;
				cache[found_index].lru = 0;
				cache[found_index].hit_info = hit_info;
			}
			return found_index;
		}

		HitInfo fetch_hit(uint cache_index) {
			return cache[cache_index].hit_info;
		}

		State get_state(uint cache_index) {
			return cache[cache_index].state;
		}

		void update_state(uint cache_index, State state) {
			cache[cache_index].state = state;
		}

		void check_cache() {
			std::map<int, int> counter;
			for (int i = 0; i < cache_size; i++) {
				if(cache[i].state != State::EMPTY) counter[cache[i].hit_info.hit_address] += 1;
			}
			for (auto [key, value] : counter) assert(value == 1);
		}

	};

	class HitRecordUpdaterRequestCrossBar : public CasscadedCrossBar<HitRecordUpdaterRequest> {
	public:
		HitRecordUpdaterRequestCrossBar(uint ports, uint channels, paddr_t hit_record_start_address) : CasscadedCrossBar<HitRecordUpdaterRequest>(ports, channels, channels), hit_record_start_address(hit_record_start_address){}
		uint get_sink(const HitRecordUpdaterRequest& request) override {
			paddr_t hit_record_address = request.hit_info.hit_address;
			int channel_id = calcDramAddr(hit_record_address).channel;
			return calcDramAddr(hit_record_address).channel;
		}
		paddr_t hit_record_start_address;
	};

	class ReturnCrossBar : public CasscadedCrossBar<MemoryReturn>
	{
	public:
		ReturnCrossBar(uint ports, uint banks) : CasscadedCrossBar<MemoryReturn>(banks, ports, banks) {}

		uint get_sink(const MemoryReturn& ret) override
		{
			return ret.port;
		}
	};

	struct Channel {
		HitRecordCache hit_record_cache; //128 records
		std::queue<int> read_queue; //128 * 7 = 1024 bit
		std::queue<int> write_queue; // TO DO: maximum queue size
	};

private:
	paddr_t hit_record_start_address;
	HitRecordUpdaterRequestCrossBar request_network;
	
	std::vector<Channel> channels;
	UnitMemoryBase* main_memory;
	uint                main_mem_port_offset{ 0 };
	uint                main_mem_port_stride{ 1 };

	uint busy = 0;
	
private:
	void process_requests(uint channel_index) {
		if (!request_network.is_read_valid(channel_index)) return;
		Channel& channel = channels[channel_index];
		HitRecordUpdaterRequest req = request_network.peek(channel_index);
		
		if (busy == 0) simulator->units_executing++;
		busy |= (1 << channel_index);

		uint cache_index = channel.hit_record_cache.look_up(req.hit_info);
		HitRecordCache::State state;
		if(cache_index != ~0) state = channel.hit_record_cache.get_state(cache_index);

		if (req.type == HitRecordUpdaterRequest::TYPE::LOAD) {
			if (state == HitRecordCache::State::ALREADY_CLOEST) {

			}
			else {

			}
		}
		else {

		}

		uint cache_index = channel.hit_record_cache.look_up(req.hit);
		if (cache_index != ~0) {
			channel.hit_record_cache.replace(req.hit); // try to replace
			request_network.read(channel_index);
		}
		else {
			uint success_index = channel.hit_record_cache.insert(req.hit);
			if (success_index != ~0) {
				request_network.read(channel_index);
				channel.read_queue.push(success_index);
			}
		}
	}

	void process_returns(uint channel_index) {
		uint port_in_main_memory = channel_index * main_mem_port_stride + main_mem_port_offset;
		if (!main_memory->return_port_read_valid(port_in_main_memory)) return;

		if (busy == 0) simulator->units_executing++;
		busy |= (1 << channel_index);

		Channel& channel = channels[channel_index];
		MemoryReturn ret = main_memory->read_return(port_in_main_memory);
		rtm::Hit hit_in_dram;
		std::memcpy(&hit_in_dram, ret.data, sizeof(rtm::Hit));

		uint cache_index = channel.hit_record_cache.compare_dram(hit_in_dram);
		if (cache_index != ~0) channel.write_queue.push(cache_index);

		channel.hit_record_cache.check_cache();
	}

	void issue_requests(uint channel_index) {
		uint port_in_main_memory = channel_index * main_mem_port_stride + main_mem_port_offset;
		if (!main_memory->request_port_write_valid(port_in_main_memory)) return;
		Channel& channel = channels[channel_index];
		
		// Write first
		bool requested = false;
		if (!channel.write_queue.empty()) {
			static uint smaller_than_dram = 0;
			uint cache_index = channel.write_queue.front();
			channel.write_queue.pop();
			channel.hit_record_cache.update_state(cache_index, HitRecordCache::State::EMPTY);

			//smaller_than_dram += 1;

			rtm::Hit hit_record = channel.hit_record_cache.fetch_hit(cache_index);
			MemoryRequest req;
			req.type = MemoryRequest::Type::STORE;
			req.port = port_in_main_memory;
			req.size = sizeof(rtm::Hit);
			req.write_mask = generate_nbit_mask(sizeof(rtm::Hit));
			req.paddr = hit_record_start_address + hit_record.id * sizeof(rtm::Hit);
			memcpy(req.data, &hit_record, sizeof(rtm::Hit));
			main_memory->write_request(req, port_in_main_memory);
			requested = true;

			/*std::cout << smaller_than_dram << '\n';*/
		}

		// Read (Only do that when we do not send a write request)
		if (!requested && !channel.read_queue.empty()) {
			uint cache_index = channel.read_queue.front();
			channel.read_queue.pop();
			channel.hit_record_cache.update_state(cache_index, HitRecordCache::State::READ_ISSUED);
			rtm::Hit hit_record = channel.hit_record_cache.fetch_hit(cache_index);
			MemoryRequest req;
			req.type = MemoryRequest::Type::LOAD;
			req.port = port_in_main_memory;
			req.size = sizeof(rtm::Hit);
			req.paddr = hit_record_start_address + hit_record.id * sizeof(rtm::Hit);
			main_memory->write_request(req, port_in_main_memory);
			requested = true;
		}
		//channel.hit_record_cache.check_cache();

		if (!requested) {
			if(busy == (1 << channel_index)) simulator->units_executing--;
			busy &= ~(1 << channel_index);
		}
	}

public:
	UnitHitRecordUpdater(Configuration config) : request_network(config.num_tms, NUM_DRAM_CHANNELS, config.hit_record_start), main_memory(config.main_mem), main_mem_port_offset(config.main_mem_port_offset), main_mem_port_stride(config.main_mem_port_stride), hit_record_start_address(config.hit_record_start){
		for (int i = 0; i < NUM_DRAM_CHANNELS; i++) {
			channels.push_back({ HitRecordCache(config.cache_size, config.associativity) });
		}
		busy = 0;
	}

	bool request_port_write_valid(uint port_index)
	{
		return request_network.is_write_valid(port_index);
	}

	void write_request(const HitRecordUpdaterRequest& request, uint port_index)
	{
		request_network.write(request, port_index);
	}

	// Process requests from TMs and returns from DRAM
	void clock_rise() override {
		request_network.clock();
		for (int i = 0; i < channels.size(); i++) {
			process_requests(i);
			process_returns(i);
		}
	}

	void clock_fall() override {
		for (int i = 0; i < channels.size(); i++) {
			issue_requests(i);
		}
		// There are no return interconnects
		
	}
};

}
}
}
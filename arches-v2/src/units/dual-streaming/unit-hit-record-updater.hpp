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
			READ_COMPLETE, // Get data from DRAM and wait to write
			READ_ISSUED,
			UNISSUED,
			EMPTY,
		};
	private:
		struct CacheElement {
			State state;
			rtm::Hit hit;
		};
		std::vector<CacheElement> cache;
		uint cache_size; // How many hit records
		uint associativity;
		uint num_set;
	public:
		HitRecordCache(uint _cache_size, uint _associativity): cache_size(_cache_size), associativity(_associativity), num_set(_cache_size / _associativity){
			cache.resize(cache_size, { State::EMPTY });
		}
		
		uint look_up(rtm::Hit hit_record, bool from_dram = false) {
			uint set_id = hit_record.id % num_set;
			uint start_index = set_id * associativity, end_index = start_index + associativity;
			assert(end_index <= cache_size);
			for (int i = start_index; i < end_index; i++) {
				if (cache[i].state != State::EMPTY && cache[i].hit.id == hit_record.id) return i;
			}
			return ~0;
		}

		void replace(rtm::Hit hit_record) {
			uint set_id = hit_record.id % num_set;
			uint start_index = set_id * associativity, end_index = start_index + associativity;
			assert(end_index <= cache_size);
			bool find_element = false;
			for (int i = start_index; i < end_index; i++) {
				if (cache[i].state != State::EMPTY && cache[i].hit.id == hit_record.id) {
					find_element = true;
					if(hit_record.t < cache[i].hit.t) cache[i].hit = hit_record;
				}
			}
			assert(find_element);
		}

		/*!
		* \brief Hit record from DRAM.
		*/
		uint compare_dram(rtm::Hit hit_record) {
			uint set_id = hit_record.id % num_set;
			uint start_index = set_id * associativity, end_index = start_index + associativity;
			assert(end_index <= cache_size);
			bool find_element = false;
			for (int i = start_index; i < end_index; i++) {
				if (cache[i].state == State::READ_ISSUED && cache[i].hit.id == hit_record.id) {
					find_element = true;
					cache[i].state = State::READ_COMPLETE;
					if (cache[i].hit.t < hit_record.t) { // The record in cache is closer
						return i;
					}
				}
			}
			assert(find_element);
			return ~0;
		}

		bool insert(rtm::Hit hit_record) {
			uint set_id = hit_record.id % num_set;
			uint start_index = set_id * associativity, end_index = start_index + associativity;
			assert(end_index <= cache_size);
			for (int i = start_index; i < end_index; i++) {
				if (cache[i].state == State::EMPTY) {
					cache[i] = { State::UNISSUED, hit_record };
					return true;
				}
			}
			return false;
		}

		rtm::Hit fetch_hit(uint cache_index) {
			return cache[cache_index].hit;
		}

		void update_state(uint cache_index, State state) {
			cache[cache_index].state = state;
		}

	};

	class HitRecordUpdaterRequestCrossBar : public CasscadedCrossBar<rtm::Hit> {
	public:
		HitRecordUpdaterRequestCrossBar(uint ports, uint channels) : CasscadedCrossBar<rtm::Hit>(ports, channels, channels) {}
		uint get_sink(const rtm::Hit& request) override {
			paddr_t hit_record_address = hit_record_start_address + request.id * sizeof(rtm::Hit);
			return hit_record_address % num_sinks();
		}
	};

	struct Channel {
		HitRecordCache hit_record_cache; //128 records
		std::queue<int> read_queue; //128 * 7 = 1024 bit
		std::queue<int> write_queue; // TO DO: maximum queue size
	};

private:
	static paddr_t hit_record_start_address;
	HitRecordUpdaterRequestCrossBar request_network;
	std::vector<Channel> channels;
	UnitMemoryBase* main_memory;
	uint                main_mem_port_offset{ 0 };
	uint                main_mem_port_stride{ 1 };
private:
	void process_requests(uint channel_index) {
		if (!request_network.is_read_valid(channel_index)) return;
		Channel& channel = channels[channel_index];
		rtm::Hit req = request_network.peek(channel_index);

		uint cache_index = channel.hit_record_cache.look_up(req);
		if (cache_index != ~0) {
			channel.hit_record_cache.replace(req); // try to replace
			request_network.read(channel_index);
		}
		else {
			bool success = channel.hit_record_cache.insert(req);
			if (success) request_network.read(channel_index);
		}
	}

	void process_returns(uint channel_index) {
		uint port_in_main_memory = channel_index * main_mem_port_stride + main_mem_port_offset;
		if (!main_memory->return_port_read_valid(port_in_main_memory)) return;
		Channel& channel = channels[channel_index];
		MemoryReturn ret = main_memory->read_return(port_in_main_memory);
		rtm::Hit hit_in_dram;
		std::memcpy(&hit_in_dram, ret.data, sizeof(rtm::Hit));

		uint cache_index = channel.hit_record_cache.compare_dram(hit_in_dram);
		if (cache_index != ~0) channel.write_queue.push(cache_index);
	}

	void issue_requests(uint channel_index) {
		uint port_in_main_memory = channel_index * main_mem_port_stride + main_mem_port_offset;
		if (!main_memory->request_port_write_valid(port_in_main_memory)) return;
		Channel& channel = channels[channel_index];
		
		// Write first
		bool requested = false;
		if (!channel.write_queue.empty()) {
			uint cache_index = channel.write_queue.front();
			channel.write_queue.pop();
			channel.hit_record_cache.update_state(cache_index, HitRecordCache::State::EMPTY);
			rtm::Hit hit_record = channel.hit_record_cache.fetch_hit(cache_index);
			MemoryRequest req;
			req.type = MemoryRequest::Type::STORE;
			req.port = port_in_main_memory;
			req.size = sizeof(rtm::Hit);
			req.write_mask = generate_nbit_mask(sizeof(rtm::Hit));
			req.paddr = hit_record_start_address + hit_record.id * sizeof(rtm::Hit);
			memcpy(req.data, &hit_record, sizeof(rtm::Hit));
			main_memory->write_request(req, port_in_main_memory);
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
		}
	}

public:
	UnitHitRecordUpdater(Configuration config) : request_network(config.num_tms, DRAM_CHANNELS), main_memory(config.main_mem), main_mem_port_offset(config.main_mem_port_offset), main_mem_port_stride(config.main_mem_port_stride) {
		hit_record_start_address = config.hit_record_start;
		for (int i = 0; i < DRAM_CHANNELS; i++) {
			channels.push_back({ HitRecordCache(config.cache_size, config.associativity) });
		}
	}

	bool request_port_write_valid(uint port_index)
	{
		return request_network.is_write_valid(port_index);
	}

	void write_request(const rtm::Hit& request, uint port_index)
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
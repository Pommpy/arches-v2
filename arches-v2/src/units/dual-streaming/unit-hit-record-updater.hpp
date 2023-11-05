#pragma once

#include "../unit-base.hpp"
#include "../../simulator/transactions.hpp"
#include "../../simulator/interconnects.hpp"
#include "../unit-dram.hpp"



namespace Arches { namespace Units { namespace DualStreaming {

uint LOG_65536[1 << 16];
uint LOG(UINT64 value) {
	assert(value != 0);
	assert(value & (value - 1) == 0); // It's a power number
	if (LOG_65536[1 << 1] != 1) {
		// Initialization
		for (uint i = 0; i < 16; i++) LOG_65536[1 << i] = i;
	}
	while ((value >> 16) > 0) value >>= 16;
	return LOG_65536[value];
}

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
			CLOSEST_WRITE,
			CLOSEST_DRAM,
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

		void compare_replace(HitInfo hit_info) {
			// This must come from a write request
			uint set_id = hit_info.hit_address % num_set;
			uint start_index = set_id * associativity, end_index = start_index + associativity;
			assert(end_index <= cache_size);
			uint found_index = ~0;
			for (int i = start_index; i < end_index; i++) {
				if (cache[i].state != State::EMPTY && cache[i].hit_info.hit_address == hit_info.hit_address) {
					found_index = i;
					if (hit_info.hit.t < cache[i].hit_info.hit.t) {
						cache[i].state = State::CLOSEST_WRITE;
						cache[i].hit_info = hit_info;
					}
				}
			}
			assert(found_index != ~0);
		}

		void direct_replace(HitInfo hit_info, uint cache_index) {
			if (cache_index != ~0) {
				cache[cache_index].state = State::LOAD_FROM_DRAM;
				cache[cache_index].lru = 0;
				cache[cache_index].hit_info = hit_info;
			}
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
					if (cache[i].hit_info.hit.t < hit_info.hit.t) { // The record in cache is closer
						cache[i].state = State::CLOSEST_WRITE;
					}
					else {
						cache[i].hit_info = hit_info;
						cache[i].state = State::CLOSEST_DRAM;
					}
				}
			}
			assert(found_index != ~0);
			return found_index;
		}

		uint try_insert(HitInfo hit_info) {
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
					if ((cache[i].state == State::CLOSEST_DRAM || cache[i].state == State::CLOSEST_WRITE) && cache[i].lru >= lru) {
						found_index = i;
						lru = cache[i].lru;
					}
				}
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
		std::queue<MemoryRequest> read_queue; //128 * 7 = 1024 bit
		std::queue<MemoryRequest> write_queue;
		std::queue<MemoryReturn> return_queue;
		std::map<paddr_t, UINT64> rsb_load_queue; // If the number of TMs is smaller than 64, otherwise we should replace UINT64 with std::vector
	};

private:
	paddr_t hit_record_start_address;
	HitRecordUpdaterRequestCrossBar request_network;
	FIFOArray<MemoryReturn> return_network;

	std::vector<Channel> channels;
	UnitMemoryBase* main_memory;
	uint                main_mem_port_offset{ 0 };
	uint                main_mem_port_stride{ 1 };

	uint busy = 0;
	
private:
	void process_requests(uint channel_index) {
		if (!request_network.is_read_valid(channel_index)) return;
		Channel& channel = channels[channel_index];
		const HitRecordUpdaterRequest rsb_req = request_network.peek(channel_index);
		
		if (busy == 0) simulator->units_executing++;
		busy |= (1 << channel_index);

		uint cache_index = channel.hit_record_cache.look_up(rsb_req.hit_info);
		HitRecordCache::State state = HitRecordCache::State::EMPTY;
		if(cache_index != ~0) state = channel.hit_record_cache.get_state(cache_index);

		if (rsb_req.type == HitRecordUpdaterRequest::TYPE::LOAD) {
			assert(fabs(rsb_req.hit_info.hit.t - T_MAX) < 1e-6); // For read request, hit.t = T_MAX
			if (state == HitRecordCache::State::CLOSEST_DRAM || state == HitRecordCache::State::CLOSEST_WRITE) {
				// We do not need to visit DRAM because we already have the closest hit
				HitInfo hit_info = channel.hit_record_cache.fetch_hit(cache_index);
				MemoryReturn ret;
				ret.port = rsb_req.port;
				ret.size = sizeof(rtm::Hit);
				std::memcpy(ret.data, &hit_info.hit, sizeof(rtm::Hit));
				ret.paddr = hit_info.hit_address;
				channel.return_queue.push(ret);
				request_network.read(channel_index);
			}
			else {
				if (cache_index != ~0) {
					// There is already a load request for this hit
					assert(rsb_req.port < 64);
					channel.rsb_load_queue[rsb_req.hit_info.hit_address] |= (1 << rsb_req.port);
					request_network.read(channel_index);
				}
				else {
					// We need to insert the request to the cache
					uint replace_index = channel.hit_record_cache.try_insert(rsb_req.hit_info);
					if (replace_index != ~0) {
						HitInfo hit_info_replaced = channel.hit_record_cache.fetch_hit(replace_index);
						HitRecordCache::State state_replaced = channel.hit_record_cache.get_state(replace_index);

						if (state_replaced == HitRecordCache::State::CLOSEST_WRITE) {
							// We need to write this data to DRAM
							MemoryRequest write_req;
							write_req.type = MemoryRequest::Type::STORE;
							write_req.size = sizeof(rtm::Hit);
							write_req.write_mask = generate_nbit_mask(sizeof(rtm::Hit));
							write_req.paddr = hit_info_replaced.hit_address;
							std::memcpy(write_req.data, &hit_info_replaced.hit, sizeof(rtm::Hit));
							channel.write_queue.push(write_req);
						}
						channel.hit_record_cache.direct_replace(rsb_req.hit_info, replace_index);

						MemoryRequest load_req;
						load_req.type = MemoryRequest::Type::LOAD;
						load_req.size = sizeof(rtm::Hit);
						load_req.paddr = rsb_req.hit_info.hit_address;
						channel.read_queue.push(load_req);

						channel.rsb_load_queue[rsb_req.hit_info.hit_address] |= (1 << rsb_req.port);
						request_network.read(channel_index);
					}
				}
				
			}
		}
		else if (rsb_req.type == HitRecordUpdaterRequest::TYPE::STORE) {
			if (cache_index != ~0) {
				// There is a record in the cache
				channel.hit_record_cache.compare_replace(rsb_req.hit_info);
				request_network.read(channel_index);
			}
			else {
				// It is a new record
				uint replace_index = channel.hit_record_cache.try_insert(rsb_req.hit_info);
				if (replace_index != ~0) {
					HitInfo hit_info_replaced = channel.hit_record_cache.fetch_hit(replace_index);
					HitRecordCache::State state_replaced = channel.hit_record_cache.get_state(replace_index);

					if (state_replaced == HitRecordCache::State::CLOSEST_WRITE) {
						// We need to write this data to DRAM
						MemoryRequest write_req;
						write_req.type = MemoryRequest::Type::STORE;
						write_req.size = sizeof(rtm::Hit);
						write_req.write_mask = generate_nbit_mask(sizeof(rtm::Hit));
						write_req.paddr = hit_info_replaced.hit_address;
						std::memcpy(write_req.data, &hit_info_replaced.hit, sizeof(rtm::Hit));
						channel.write_queue.push(write_req);
					}
					channel.hit_record_cache.direct_replace(rsb_req.hit_info, replace_index);
					
					MemoryRequest load_req;
					load_req.type = MemoryRequest::Type::LOAD;
					load_req.size = sizeof(rtm::Hit);
					load_req.paddr = rsb_req.hit_info.hit_address;
					channel.read_queue.push(load_req);

					request_network.read(channel_index);
				}
			}
		}
		else assert(false);
	}

	void process_returns(uint channel_index) {
		uint port_in_main_memory = channel_index * main_mem_port_stride + main_mem_port_offset;
		if (!main_memory->return_port_read_valid(port_in_main_memory)) return;

		if (busy == 0) simulator->units_executing++;
		busy |= (1 << channel_index);

		Channel& channel = channels[channel_index];
		MemoryReturn ret_from_dram = main_memory->read_return(port_in_main_memory);
		paddr_t hit_address = ret_from_dram.paddr;
		rtm::Hit hit_in_dram;
		std::memcpy(&hit_in_dram, ret_from_dram.data, sizeof(rtm::Hit));
		HitInfo hit_info = { hit_in_dram, hit_address };

		uint cache_index = channel.hit_record_cache.process_dram_hit(hit_info);
		if (cache_index != ~0) {
			HitInfo cloest_hit_info = channel.hit_record_cache.fetch_hit(cache_index);
			// If there are load requests from TP
			if (channel.rsb_load_queue.count(hit_address)) {
				UINT64 rsb_set = channel.rsb_load_queue[hit_address];
				for (; rsb_set != 0; rsb_set -= (rsb_set & -rsb_set)) {
					uint rsb_index = LOG(rsb_set & -rsb_set);
					MemoryReturn ret_to_rsb;
					ret_to_rsb.paddr = hit_address;
					ret_to_rsb.port = rsb_index;
					std::memcpy(ret_to_rsb.data, &hit_info.hit, sizeof(rtm::Hit));
					channel.return_queue.push(ret_to_rsb);
				}
				channel.rsb_load_queue.erase(hit_address);
			}
		}
		else assert(false); // there must be a record in the cache
		
	}

	void issue_requests(uint channel_index) {
		uint port_in_main_memory = channel_index * main_mem_port_stride + main_mem_port_offset;
		if (!main_memory->request_port_write_valid(port_in_main_memory)) return;
		Channel& channel = channels[channel_index];
		
		// Read first
		bool requested = false;
		if (!channel.read_queue.empty()) {
			MemoryRequest req = channel.read_queue.front();
			channel.read_queue.pop();
			main_memory->write_request(req, port_in_main_memory);
			requested = true;
		}

		// Read (Only do that when we do not send a write request)
		if (!requested && !channel.write_queue.empty()) {
			MemoryRequest req = channel.write_queue.front();
			channel.write_queue.pop();
			main_memory->write_request(req, port_in_main_memory);
			requested = true;
		}

		if (!requested) {
			if(busy == (1 << channel_index)) simulator->units_executing--;
			busy &= ~(1 << channel_index);
		}
	}

	void issue_returns(uint channel_index) {
		Channel& channel = channels[channel_index];
		auto& queue = channel.return_queue;
		if (!queue.empty()) {
			MemoryReturn ret = queue.front();
			if (return_network.is_write_valid(ret.port)) {
				return_network.write(ret, ret.port);
				queue.pop();
			}
		}
	}

public:
	UnitHitRecordUpdater(Configuration config) : request_network(config.num_tms, NUM_DRAM_CHANNELS, config.hit_record_start), main_memory(config.main_mem), return_network(config.num_tms), main_mem_port_offset(config.main_mem_port_offset), main_mem_port_stride(config.main_mem_port_stride), hit_record_start_address(config.hit_record_start){
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

	bool return_port_read_valid(uint port_index)
	{
		return return_network.is_read_valid(port_index);
	}

	const MemoryReturn& peek_return(uint port_index)
	{
		return return_network.peek(port_index);
	}

	const MemoryReturn read_return(uint port_index)
	{
		return return_network.read(port_index);
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
			issue_returns(i);
		}
		// There are no return interconnects
		return_network.clock();
	}
};

}
}
}
#pragma once
#include "../../stdafx.hpp"

#include "../util/arbitration.hpp"

struct alignas(64) _64Aligned
{
	uint8_t _u8[64];
	_64Aligned()
	{
		for(uint i = 0; i < 64; ++i) _u8[i] = 0u;
	}

	uint8_t& operator[](uint index)
	{
		return _u8[index];
	}
};

template<typename RET>
class Bus
{
private:
	//Should be much more efficent since each line has one byte and they are alligne they will likely be on the same line
	//reads to the data only happen if pending flag is set and ack is done by clearing the pending bit
	std::vector<_64Aligned> _pending;
	std::vector<RET>          _data;

public:
	Bus(uint size)
	{
		_pending.resize((size + 63) / 64);
		_data.resize(size);
	}

	~Bus()
	{
	}

	uint size() const { return _data.size(); }

	bool transfer_pending(uint index) const
	{
		uint8_t* pending = (uint8_t*)_pending.data();

		assert(index < size());

		return pending[index];
	}
	void acknowlege(uint index)
	{
		uint8_t* pending = (uint8_t*)_pending.data();

		assert(index < size());
		assert(pending[index]);

		pending[index] = 0;
	}
	const RET& transfer(uint index) const
	{
		uint8_t* pending = (uint8_t*)_pending.data();

		assert(index < size());
		assert(pending[index]);

		return _data[index];
	}
	void add_transfer(const RET& data, uint index)
	{
		uint8_t* pending = (uint8_t*)_pending.data();

		assert(index < size());
		assert(!pending[index]);

		_data[index] = data;
		pending[index] = 1;
	}
};

//up to 64 client and servers
template<typename REQ, typename RET>
class InterconnectionNetwork
{
private:
	_64Aligned _request_pending; //these are used as a thread safe way of setting pending bit
	_64Aligned _return_ack;

	std::vector<REQ>                    _request_regs; //the requests but
	std::vector<RoundRobinArbitrator64> _request_arbs; //the arbitrator for putting incoming request on the data lines

	std::vector<RET>                    _return_regs; //the return regs
	std::vector<RoundRobinArbitrator64> _return_arbs; //the arbitrators for selecting which request to service
	uint64_t _server_pending_mask;

public:
	InterconnectionNetwork(uint clients, uint servers)
	{
		_request_regs.resize(clients);
		_return_regs.resize(servers);
		_request_arbs.resize(servers);
		_return_arbs.resize(clients);
	}

	//SERVER INTERFACE
	//Clock rise
	//Must call at the begining of clock rise. Propagates requests
	void propagate_requests(uint (*routing_func)(const REQ& req, uint client, uint num_clients, uint num_servers))
	{
		for(uint client = 0; client < _request_regs.size(); ++client)
		{
			if(!_request_pending[client]) continue;
			uint server = routing_func(_request_regs[client], client, _request_regs.size(), _return_regs.size());
			_request_arbs[server].add(client);
		}

		for(uint server = 0; server < _request_arbs.size(); ++server)
			_request_arbs[server].update();
	}

	const REQ* get_request(uint server, uint& client)
	{
		client = _request_arbs[server].current();
		if(client == ~0) return nullptr;
		return &_request_regs[client];
	}

	void acknowlege_request(uint server, uint client)
	{
		_request_arbs[server].remove(client);
		_request_pending[client] = 0;
	}

	uint64_t merge_requests(const REQ& request)
	{
		uint64_t client_mask = 0;
		for(uint i = 0; i < _request_regs.size(); ++i)
			if(_request_pending[i] && _request_regs[i] == request)
				client_mask |= 0x1ull << i;

		return client_mask;
	}

	void acknowlege_requests(uint server, uint64_t client_mask)
	{
		for(uint client = 0; client < _request_regs.size(); ++client)
		{
			if(!((client_mask >> client) & 0x1ull)) continue;
			acknowlege_request(server, client);
		}
	}

	uint num_request_pending(uint server)
	{
		return _request_arbs[server].num_pending();
	}

	//Clock rise
	//Must call at begining of clock fall. Propagates acks
	void propagate_ack()
	{
		for(uint client = 0; client < _return_arbs.size(); ++client)
		{
			uint server = _return_arbs[client].current();
			if(server == ~0 || !_return_ack[client]) continue;

			_return_ack[client] = 0;
			_return_arbs[client].remove(server);
		}

		_server_pending_mask = 0;
		for(uint client = 0; client < _request_regs.size(); ++client)
			_server_pending_mask |= _return_arbs[client].pending_mask();
	}

	void add_return(const RET& data, uint server, uint client)
	{
		_return_regs[server] = data;
		_return_arbs[client].add(server);
	}

	void add_returns(const RET& data, uint server, uint64_t client_mask)
	{
		_return_regs[server] = data;
		for(uint client = 0; client < _return_arbs.size(); ++client)
		{
			if(!((client_mask >> client) & 0x1ull)) continue;
			_return_arbs[client].add(server);
		}
	}

	bool return_pending(uint server)
	{
		return (_server_pending_mask >> server) & 0x1ull;
	}

	//Must call at end of clock fall. Propagates returns
	void propagate_returns()
	{
		for(uint client = 0; client < _return_arbs.size(); ++client)
			_return_arbs[client].update();
	}

	//CLIENT INTERFACE
	//clock rise
	const RET* get_return(uint client)
	{
		uint server = _return_arbs[client].current();
		if(server == ~0) return nullptr;
		return &_return_regs[server];
	}

	void acknowlege_return(uint client)
	{
		_return_ack[client] = 1;
	}

	//clock fall
	void add_request(const REQ& req, uint client)
	{
		_request_regs[client] = req;
		_request_pending[client] = 1;
	}

	bool request_pending(uint client)
	{
		return _request_pending[client];
	}
};
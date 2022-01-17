#pragma once

#include "../../stdafx.hpp"

namespace Arches { namespace Util {

#if 1
//you need two
class barrier final {
	private:
		std::atomic_size_t _count;

	public:
		barrier() {}
		~barrier() = default;

		void operator()(size_t num_threads)
		{
			++_count;
			while(_count < num_threads);
			++_count;
			if(_count == (num_threads * 2))	_count = 0;
		}
};


#else
class barrier final {
private:
	std::mutex _mutex;
	std::condition_variable _cv;

	size_t _count;

public:
	barrier() : _count(0) {}
	~barrier() = default;

	void operator()(size_t num_threads)
	{
		std::unique_lock<std::mutex> lock(_mutex);

		++_count;

		if(_count < num_threads) {
			_cv.wait(lock);
		}
		else {
			_count = 0;
			_cv.notify_all();
		}
	}
};
#endif

}}

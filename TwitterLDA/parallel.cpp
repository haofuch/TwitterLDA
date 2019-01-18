#include "parallel.h"
#include <thread>
#include <condition_variable>
#include <mutex>

parallel::parallel(size_t thread_num)
{
	_thread_num = thread_num;
	_threads = nullptr;
}

parallel::~parallel()
{
	if (_threads == nullptr) return;

	for (size_t i = 0; i < _thread_num; ++i)
	{
		std::unique_lock<std::mutex> lock(_mutexes[i]);
		_signals[i] = _signal::exit;
		_cvs1[i].notify_one();
	}
	for (size_t i = 0; i < _thread_num; ++i)
	{
		_threads[i].join();
	}
	delete[] _mutexes;
	delete[] _signals;
	delete[] _threads;
	delete[] _cvs1;
	delete[] _cvs2;
}

void parallel::_init()
{
	if (_threads != nullptr) return;

	_threads = new std::thread[_thread_num];
	_cvs1 = new std::condition_variable[_thread_num];
	_cvs2 = new std::condition_variable[_thread_num];
	_signals = new _signal[_thread_num];
	_mutexes = new std::mutex[_thread_num];
	for (size_t i = 0; i < _thread_num; ++i)
	{
		_signals[i] = _signal::suspend;
		_threads[i] = std::thread(&parallel::_update_worker, this, i);
	}
}

void parallel::_update()
{
	_init();

	// notify worker threads to start
	for (size_t i = 0; i < _thread_num; ++i)
	{
		std::unique_lock<std::mutex> lock(_mutexes[i]);
		_signals[i] = _signal::run;
		_cvs1[i].notify_one();
	}

	// wait for worker threads to finish
	for (size_t i = 0; i < _thread_num; ++i)
	{
		std::unique_lock<std::mutex> lock(_mutexes[i]);
		while (_signals[i] != _signal::suspend) _cvs2[i].wait(lock);
	}
}

void parallel::_update_worker(size_t id)
{
	while (true)
	{
		{
			std::unique_lock<std::mutex> lock(_mutexes[id]);
			while (_signals[id] == _signal::suspend) _cvs1[id].wait(lock);
			if (_signals[id] == _signal::exit) return;
		}

		_update(id);

		{
			std::unique_lock<std::mutex> lock(_mutexes[id]);
			_signals[id] = _signal::suspend;
			_cvs2[id].notify_one();
		}
	}
}
#pragma once
#include <thread>
#include <condition_variable>
#include <mutex>

class parallel
{
public:
	parallel(size_t thread_num);
	~parallel();

protected:
	size_t _thread_num;
	std::thread *_threads;
	std::condition_variable *_cvs1, *_cvs2;
	std::mutex *_mutexes;

	enum _signal
	{
		suspend, run, exit
	};

	_signal *_signals;

	void _init();
	void _update();
	void _update_worker(size_t id);

	virtual void _update(size_t id) = 0;
};


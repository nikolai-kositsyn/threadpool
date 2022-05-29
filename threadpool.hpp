#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include <future>
#include <memory>
#include <deque>
#include <list>
#include <algorithm>

using namespace std;

namespace threading
{
//#define USE_LOG_POOL

#ifdef USE_LOG_POOL
#define LOG_POOL(...) cout << __VA_ARGS__ << endl;
#else
#define LOG_POOL(...)
#endif

	template <typename T>
	struct func_traits { };

	template <typename R, typename... Args>
	struct func_traits<R(Args...)>
	{
		using RetType = R;
	};

	using task_type = function<void()>;

	class tasksqueue
	{
		mutable mutex _mutex;
		condition_variable _cond;
		deque<task_type> _deque;

	public:
		tasksqueue() = default;
		~tasksqueue() = default;
		tasksqueue(const tasksqueue& other) = delete;
		tasksqueue(tasksqueue&& other) = delete;

		void push(task_type& task)
		{
			{
				lock_guard<mutex> lock(_mutex);
				_deque.push_back(task);
			}

			_cond.notify_one();
		}

		void pop(task_type& task)
		{
			unique_lock<mutex> lock(_mutex);

			if (_deque.empty())
			{
				_cond.wait(lock);

				if (!_deque.empty())
				{
					task = _deque.front();
					_deque.pop_front();
				}

			}
			else
			{
				task = _deque.front();
				_deque.pop_front();
			}
		}

		size_t size() const noexcept
		{
			lock_guard<mutex> lock(_mutex);
			return _deque.size();
		}

		void notify_all_workers() noexcept
		{
			_cond.notify_all();
		}
	};

	class worker
	{
		tasksqueue& _queue;

		thread _thread;
		mutex _mutex;
		condition_variable _cond;
		atomic<bool> _stop_request = { false };
		bool _thread_func_finished = { false };

		mutex _timer_mutex;
		condition_variable _timer_cond;
		bool _stop_timer_request = { true };

		atomic<bool> _is_busy = { false };

		void run()
		{
			while (true)
			{
				task_type task = nullptr;

				_queue.pop(task);

				if (task != nullptr)
				{
					_is_busy.store(true, memory_order_relaxed);

					task();

					_is_busy.store(false, memory_order_relaxed);
				}

				if (_stop_request)
				{
					break;
				}
			}

			{
				lock_guard<mutex> lock(_mutex);
				_thread_func_finished = true;

#ifdef USE_LOG_POOL
				stringstream msg;
				msg << "worker:" << get_id() << " | finish flag set";
				LOG_POOL(msg.str());
#endif
			}

			_cond.notify_one();

#ifdef USE_LOG_POOL
			stringstream msg;
			msg << "worker:" << get_id() << " | finish cond.notify()";
			LOG_POOL(msg.str());
#endif			
		}

	public:
		worker() = delete;
		worker(worker& other) = delete;
		worker(worker&& other) = delete;

		worker(tasksqueue& queue)
			: _queue(queue)
		{
			_thread = thread(&worker::run, this);
		}

		~worker()
		{
			if (_thread.joinable())
			{
				constexpr auto WAIT_ATTEMPTS_COUNT = 10;
				constexpr chrono::milliseconds SINGLE_WAIT_TIMEOUT_MS(200);

				const auto thread_id = get_id();

				/* Stop and waiting for a timer */
				for (auto attempt = 1; attempt <= WAIT_ATTEMPTS_COUNT; ++attempt)
				{
					set_stop_timer_request();

					if (wait_for_stop_timer_request(SINGLE_WAIT_TIMEOUT_MS))
					{
#ifdef USE_LOG_POOL
						stringstream msg;
						msg << "worker:" << thread_id << " | timer waited successfully, attempt:" << attempt;
						LOG_POOL(msg.str());
#endif

						break;
					}
				}

				/* Stop and waiting for a worker thread */
				_stop_request = true;

				unique_lock<mutex> lock(_mutex);
				for (auto attempt = 1; attempt <= WAIT_ATTEMPTS_COUNT; ++attempt)
				{
					_queue.notify_all_workers();

					auto waitStatus = _cond.wait_for(lock, SINGLE_WAIT_TIMEOUT_MS);
					if (waitStatus == cv_status::no_timeout)
					{
#ifdef USE_LOG_POOL
						stringstream msg;
						msg << "worker:" << thread_id << " | waited successfully, attempt:" << attempt;
						LOG_POOL(msg.str());
#endif

						break;
					}
				}

				if (_thread_func_finished)
				{
					_thread.join();

#ifdef USE_LOG_POOL
					stringstream msg;
					msg << "worker:" << thread_id << " | joined with wait";
					LOG_POOL(msg.str());
#endif					
				}
				else
				{
					/* Detaching worker thread... something went wrong */
#ifdef USE_LOG_POOL
					{
						stringstream msg;
						msg << "worker:" << thread_id << " | going to 'detach' thread... something went wrong";
						LOG_POOL(msg.str());
					}
#endif

					_thread.detach();

#ifdef USE_LOG_POOL
					{
						stringstream msg;
						msg << "worker:" << thread_id << " | detached";
						LOG_POOL(msg.str());
					}
#endif
				}
			}
		}

		thread::id get_id() const noexcept
		{
			return _thread.get_id();
		}

		bool is_busy() const noexcept
		{
			return _is_busy;
		}

		void set_stop_timer_request() noexcept
		{
			{
				lock_guard<mutex> lock(_timer_mutex);
				_stop_timer_request = true;
			}

			_timer_cond.notify_one();
		}

		void clear_stop_timer_request() noexcept
		{
			lock_guard<mutex> lock(_timer_mutex);
			_stop_timer_request = false;
		}

		bool wait_for_stop_timer_request(chrono::milliseconds timeout) noexcept
		{
			unique_lock<mutex> lock(_timer_mutex);
			if (!_stop_timer_request)
			{
				_timer_cond.wait_for(lock, timeout);
			}

			return _stop_timer_request;
		}
	};

	struct pool_stat
	{
		size_t numOfThreads;
		size_t numOfBusyThreads;
		size_t numOfPendingTasks;
	};

	class threadpool
	{
		tasksqueue _tasks_queue;
		mutable mutex _mutex;
		list<unique_ptr<worker>> _workers;

		void timer_func(promise<thread::id>& promise,
			task_type callback,
			chrono::milliseconds interval,
			chrono::milliseconds startDelay,
			bool singleShot)
		{
			auto threadId = this_thread::get_id();

			auto workerIter = find_if(_workers.begin(), _workers.end(),
				[&](const unique_ptr<worker>& item) { return item->get_id() == threadId; });

			auto workerItem = workerIter->get();

			workerItem->clear_stop_timer_request();

			promise.set_value(threadId);

			if (workerItem->wait_for_stop_timer_request(startDelay))
			{
				return;
			}

			if (singleShot)
			{
				callback();
			}
			else
			{
				while (true)
				{
					auto startTime = chrono::high_resolution_clock::now();

					callback();

					auto endTime = chrono::high_resolution_clock::now();
					auto elapsed = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);

					auto timeToWait = chrono::milliseconds(0);
					if (elapsed < interval)
					{
						timeToWait = interval - elapsed;
					}

					if (workerItem->wait_for_stop_timer_request(timeToWait))
					{
						break;
					}
				}
			}
		}

	public:
		threadpool(size_t numOfThreads = thread::hardware_concurrency())
		{
			configure(numOfThreads);
		}


		/**
		 * @brief Configures the number of active worker threads in the pool.
		 * @param numOfThreads Target number of threads.
		 * By default that number is equal to the number of hardware threads (std::thread::hardware_concurrency()).
		 */
		void configure(size_t numOfThreads)
		{
			lock_guard<mutex> lock(_mutex);

			while (_workers.size() < numOfThreads)
			{
				unique_ptr<worker> item(new worker(_tasks_queue));
				_workers.push_back(move(item));
			}

			while (_workers.size() > numOfThreads)
			{
				_workers.pop_front();
			}
		}


		/**
		 * @brief Returns the thread pool statistics info.
		 * @return 'pool_stat' structure instance which has the following info members:
		 * 'numOfThreads' - Number of active worker threads
		 * 'numOfPendingTasks' - Number of pending tasks in the internal queue
		 */
		pool_stat get_statistics() const noexcept
		{
			pool_stat stat{};

			{
				lock_guard<mutex> lock(_mutex);

				stat.numOfThreads = _workers.size();
				for (auto& worker : _workers)
				{
					if (worker->is_busy())
					{
						stat.numOfBusyThreads++;
					}
				}
			}

			stat.numOfPendingTasks = _tasks_queue.size();

			return stat;
		}


		/**
		 * @brief Pushes the function to execute it in the thread pool.
		 * @param func Target callable object to execute in pool.
		 * @param args Arguments of callable object.
		 * @return Instance of 'future' to wait and get result of asynchronous executing task in pool.
		 */
		template<typename F, typename... Types>
		auto push_task(F&& func, Types&&... args) -> future<decltype(func(args...))>
		{
			auto callable = bind(forward<F>(func), forward<Types>(args)...);

			auto package = make_shared<packaged_task<decltype(func(args...))()>>(callable);

			auto future = package->get_future();

			task_type task([package]() { (*package)(); });
			_tasks_queue.push(task);

			return future;
		}


		/**
		 * @brief Pushes the member function to execute it in the thread pool.
		 * @param func Member function to execute in pool.
		 * @param args Arguments of member function.
		 * @return Instance of 'future' to wait and get result of asynchronous executing task in pool.
		 */
		template<typename R, typename C, typename... Types>
		auto push_task(R(C::* func), Types&&... args) -> future<typename func_traits<R>::RetType>
		{
			auto callable = bind(func, forward<Types>(args)...);

			auto package = make_shared<packaged_task<typename func_traits<R>::RetType()>>(callable);

			auto future = package->get_future();

			task_type task([package]() { (*package)(); });
			_tasks_queue.push(task);

			return future;
		}


		/**
		 * @brief Pushes the callable object to execute it like threading timer callback.
		 * @param callback Callable object for timer.
		 * @param interval Execution period of timer in milliseconds.
		 * @param startDelay Start up delay of timer in milliseconds.
		 * @param singleShot Flag to indicate 'single shot' or 'periodic' mode of timer.
		 * @return The 'thread::id' value of assotiated worker thread which took that timer task.
		 */
		thread::id push_timer_task(function<void()> callback,
			chrono::milliseconds interval,
			chrono::milliseconds startDelay = chrono::milliseconds(0),
			bool singleShot = false)
		{
			promise<thread::id> promise;
			auto future = promise.get_future();

			task_type task([&]()
				{
					timer_func(promise, callback, interval, startDelay, singleShot);
				});

			_tasks_queue.push(task);

			return future.get();
		}


		/**
		 * @brief Set stop request flag execution of threading timer assotiated with 'thread::id' value.
		 * @param threadId The 'thread::id' value of worker thread.
		 */
		void stop_timer_task(thread::id threadId)
		{
			lock_guard<mutex> lock(_mutex);

			auto workerItem = find_if(_workers.begin(), _workers.end(),
				[&](const unique_ptr<worker>& item) { return item->get_id() == threadId; });

			if (workerItem != _workers.end())
			{
				workerItem->get()->set_stop_timer_request();
			}
		}

		/**
		 * @brief Waits for a finishing all pending tasks.
		 */
		void wait_for_all_tasks()
		{
			auto stat = get_statistics();

			while (stat.numOfPendingTasks > 0 || stat.numOfBusyThreads > 0)
			{
				this_thread::yield();
				stat = get_statistics();
			}
		}
	};
}

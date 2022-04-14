#include <gtest/gtest.h>
#include <string>

#include "threadpool.hpp"

using namespace threading;

TEST(pool, configure_test)
{
	threadpool pool;
	EXPECT_EQ(thread::hardware_concurrency(), pool.get_statistics().numOfThreads);

	pool.configure(10);
	EXPECT_EQ(10, pool.get_statistics().numOfThreads);

	pool.configure(30);
	EXPECT_EQ(30, pool.get_statistics().numOfThreads);

	pool.configure(20);
	EXPECT_EQ(20, pool.get_statistics().numOfThreads);

	pool.configure(0);
	EXPECT_EQ(0, pool.get_statistics().numOfThreads);	
}

TEST(pool, get_statistics_test)
{
	auto NUM_OF_TASKS = 10;
	auto NUM_OF_THREADS_FOR_CONFIGURE = 3;

	auto lambda = [] { return 42; };

	threadpool pool(0);

	list<future<int>> results;
	for (auto i = 0; i < NUM_OF_TASKS; i++)
	{
		results.push_back(pool.push_task(lambda));
	}

	EXPECT_EQ(0, pool.get_statistics().numOfThreads);
	EXPECT_EQ(NUM_OF_TASKS, pool.get_statistics().numOfPendingTasks);

	pool.configure(NUM_OF_THREADS_FOR_CONFIGURE);
	for (auto& result : results)
	{
		EXPECT_EQ(42, result.get());
	}

	EXPECT_EQ(NUM_OF_THREADS_FOR_CONFIGURE, pool.get_statistics().numOfThreads);
	EXPECT_EQ(0, pool.get_statistics().numOfPendingTasks);
}

TEST(pool, push_task_lambda_test)
{
	threadpool pool;

	auto func_void = [] { };

	auto func_args = [](int x, int y, int z) { return x + y + z; };

	auto func_except = [] { throw std::runtime_error("Test exception"); };

	auto void_future = pool.push_task(func_void);
	auto args_future = pool.push_task(func_args, 1, 2, 3);
	auto except_future = pool.push_task(func_except);

	void_future.wait();

	EXPECT_EQ(6, args_future.get());

	try
	{
		except_future.get();
	}
	catch (const exception& ex)
	{
		EXPECT_EQ("Test exception", string(ex.what()));
	}
}

void func_void() { }

int func_args(int x, int y, int z) { return x + y + z; }

void func_except() { throw std::runtime_error("Test exception"); }

TEST(pool, push_task_func_test)
{
	threadpool pool;

	auto void_future = pool.push_task(func_void);

	auto args_future = pool.push_task(func_args, 1, 2, 3);
	auto except_future = pool.push_task(func_except);

	void_future.wait();

	EXPECT_EQ(6, args_future.get());

	try
	{
		except_future.get();
	}
	catch (const std::exception& ex)
	{
		EXPECT_EQ("Test exception", std::string(ex.what()));
	}
}

struct TestObject
{
	void Set(int numVal, string strVal)
	{
		_numVal = numVal;
		_strVal = strVal;
	}

	void Get(int& numberVal, string& strVal)
	{
		numberVal = _numVal;
		strVal = _strVal;
	}

	int operator()(int x, int y, int z) const
	{
		return x + y + z;
	}

	static int Sum(int x, int y, int z)
	{
		return x + y + z;
	}

private:
	int _numVal;
	string _strVal;
};

TEST(pool, push_task_mfunc_set_test)
{
	threadpool pool;

	TestObject obj;

	auto future = pool.push_task(&TestObject::Set, ref(obj), 111, "222");
	future.wait();

	auto actualNum = 0;
	auto actualStr = string("");
	obj.Get(actualNum, actualStr);

	EXPECT_TRUE(111 == actualNum && "222" == actualStr);
}

TEST(pool, push_task_mfunc_get_test)
{
	threadpool pool;

	TestObject obj;

	obj.Set(111, "222");

	auto actualNum = 0;
	auto actualStr = string("");
	auto future = pool.push_task(&TestObject::Get, ref(obj), ref(actualNum), ref(actualStr));
	future.wait();

	EXPECT_TRUE(111 == actualNum && "222" == actualStr);
}

TEST(pool, push_task_static_func_test)
{
	threadpool pool;

	auto sumFuture = pool.push_task(&TestObject::Sum, 1, 2, 3);
	EXPECT_EQ(6, sumFuture.get());

	auto callFuture = pool.push_task(TestObject(), 1, 2, 3);
	EXPECT_EQ(6, callFuture.get());
}

struct TimerClientTestObject
{
	threadpool& _pool;
	thread::id _timerId;
	size_t _counter;

	TimerClientTestObject(threadpool& pool)
		:_pool(pool)
	{
		_counter = 0;
	}

	void Callback()
	{
		++_counter;
	}

	size_t GetCounter()
	{
		return _counter;
	}

	void Start(chrono::milliseconds interval, chrono::milliseconds startDelay)
	{
		_timerId = _pool.push_timer_task([this] { Callback(); }, interval, startDelay, false);
	}

	void Stop()
	{
		_pool.stop_timer_task(_timerId);
	}
};

TEST(pool, push_timer_task_test)
{
	threadpool pool;

	TimerClientTestObject client(pool);

	const int EXPECTED_CALLS = 7;

	chrono::milliseconds interval{ 20 }, startDelay{ 100 };

	auto startTime = chrono::high_resolution_clock::now();

	client.Start(interval, startDelay);

	while (true)
	{
		auto counter = client.GetCounter();
		if (counter == EXPECTED_CALLS)
		{
			client.Stop();

			// success
			break;
		}

		auto currentTime = chrono::high_resolution_clock::now();
		auto elapsedTime = chrono::duration_cast<chrono::milliseconds>(currentTime - startTime);
		if (elapsedTime > (2 * (startDelay + interval * EXPECTED_CALLS)))
		{
			throw std::runtime_error("Couldn't wait for expected timer counter value");
		}
	}
}

TEST(pool, push_singleshot_timer_test)
{
	threadpool pool;

	auto flag = false;
	auto timerCallback = [&] { flag = true; };

	chrono::milliseconds interval{ 0 }, startDelay{ 100 };
	bool singleShot = true;

	auto timerThreadId = pool.push_timer_task(timerCallback, interval, startDelay, singleShot);

	stringstream stream;
	stream << timerThreadId;
	EXPECT_TRUE(stoull(stream.str()) > 0);

	// manual sleep for test purposes
	this_thread::sleep_for(2 * startDelay);

	EXPECT_TRUE(flag);
}


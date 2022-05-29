[![Ubuntu](https://github.com/nikolai-kositsyn/threadpool/actions/workflows/ubuntu.yml/badge.svg?branch=main)](https://github.com/nikolai-kositsyn/threadpool/actions/workflows/ubuntu.yml) [![Windows](https://github.com/nikolai-kositsyn/threadpool/actions/workflows/windows.yml/badge.svg?branch=main)](https://github.com/nikolai-kositsyn/threadpool/actions/workflows/windows.yml) [![CodeQL](https://github.com/nikolai-kositsyn/threadpool/actions/workflows/codeql.yml/badge.svg?branch=main)](https://github.com/nikolai-kositsyn/threadpool/actions/workflows/codeql.yml) [![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/nikolai-kositsyn/threadpool/main/LICENSE.txt) [![GitHub Downloads](https://img.shields.io/github/downloads/nikolai-kositsyn/threadpool/total)](https://github.com/nikolai-kositsyn/threadpool/releases)


# C++ Thread Pool single header library

## General Description

The threadpool is a single header and self-contained library. The implementation doesn't have any dependencies other than the C++ standard library. The source code is completely free and open source under the [MIT license](LICENSE.txt).

Actually thread pool has a very simple interface. The `threadpool.hpp` contains all necessary functionality.
The [CMake](https://cmake.org/cmake/help/latest/guide/tutorial/index.html) build system is used for build in C++ 11/14/17 projects.
The [GoogleTest](https://google.github.io/googletest/) framework is used for unit testing the library API.
See `threadpool_tests.cpp` to get library usage examples.


## Detailed API Description

### 1. Constructor and Configuration
The constructor of `threadpool` class allows to set the number of active worker threads in the pool. By default the number of threads is equal to `thread::hardware_concurrency()`. To change the number of active worker threads the `configure` function should be used. See code snippet below of `confire_test` test function.

```
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
```

### 2. Get Statistics
The `get_statistics` function returns the instance of `pool_stat` struct which shows the internal state of pool. See struct declaration below.

```
    struct pool_stat
	{
		size_t numOfThreads;
		size_t numOfBusyThreads;
		size_t numOfPendingTasks;
	};
```

### 3. Push Tasks into Pool
The library supports the pushing any callable object into the pool. The class member functions are also supported. Internally the `std::packaged_task` is used. The `push_task` function returns the `std::future` instance which can be used to wait and get results of asynchronously execute tasks. The following tests can show the typical usage examples of pushing and waiting the callable objects.

```
    - push_task_lambda_test
    - push_task_func_test
    - push_task_mfunc_set_test
    - push_task_mfunc_get_test
    - push_task_static_func_test
```

### 4. Use Threading Timers
The library supports the threading timers. To initiate timer in the pool the client should pass an callable object and other neccesarry timer parameters. The function `push_timer_task` returns the `std::thread::id` instance to stop the related timer in the future. See the following threading timers tests: `push_timer_task_test`, `push_singleshot_timer_test`.

### 5. Waiting Pending Tasks and Pool Destruction
Sometimes it is neccesarry to have ability of waiting some or all pending tasks in the thread pool explicitly. The library supports two options to do that:
    - Use the returned `std::future` instances to wait and get results
    - Use the `wait_for_all_tasks` function to wait for complition all pending tasks in the thread pool

See `wait_for_all_future_results` and `wait_for_all_tasks_test` tests to get more detailes.

The destructor of `threadpool` object initiates stopping and destruction of the all existing workers. The rest tasks which were pushed into the pool will be also destroyed. So the destructor doesn't wait for finishing the rest pending tasks.
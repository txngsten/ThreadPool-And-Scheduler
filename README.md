# Thread Pool and Task Scheduler
A thread pool that takes advantage of safe task stealing and manages false sharing efficiently through struct alignment.
Also includes a simple task scheduler which manges task dependencies through a DAG ([Directed Acyclic Graph](https://en.wikipedia.org/wiki/Directed_acyclic_graph)).
****

## Thread Pool
A C++ thread pool that implements safe and efficient job stealing, supports up to $n$ threads in the pool, safe and clean shutdown procedures, and using struct alignment to mitigate false sharing.
Work stealing is implemented through each worker thread being assigned its own deque, where the owner pops tasks from the back and thief threads can steal from the front.
Line 37 of [ThreadPool.cpp](ThreadPool.cpp) contains a variable used to limit the number of attempts a thief thread can attempt to steal before going back to sleep:
```c++
constexpr int ATTEMPTS = 4;
```
It's currently set to 4 but can be changed based on performance needs.

### API

| **Method**                            | **Description**                                                                                                 |
|---------------------------------------|-----------------------------------------------------------------------------------------------------------------|
| `ThreadPool(size_t numThreads)`       | Constructor that creates a thread pool of size numThreads. Will assign each thread its own deque and unique ID. |
| `submit(std::function<void()> task)`  | Submits a task to the thread pool, atomic counter is used to determine which worker deque the task is added to. |
| `std::vector<WorkerStats> getStats()` | Returns an array of `WorkerStats` which shows how many tasks each worker thread has successfully executed.      |

****

## DAG Task Scheduler
A simple C++ task scheduler which supports priority by time and priority value respectively.
Task dependencies are managed through a DAG adjacency list implementation ensuring that tasks are sent to the thread pool in a topological sorted order.

### Scheduled Task Object
The task scheduler interface and usage of this interface revolves around a few important data structures.
One being the scheduled task data structure defined as:
```c++
struct ScheduledTask {
        TaskID id;
        TimePoint startTime;
        int priority;
        std::function<void()> fn;
        ScheduledTask(TaskID id, TimePoint startTime, int priority, std::function<void()> fn) :
            id(id), startTime(startTime), priority(priority), fn(std::move(fn)) {}
    };
```
Some important clarifications are also:
```c++
using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;
using Clock = std::chrono::steady_clock;
using TaskID = std::uint64_t;
```
As you can see, this data structure holds a unique task ID, a time point for the earliest execution, an integer priority value, and a function (the actual task).
Understanding this implementation will make the API and usage a lot more intuitive.

### API 

| **Method**                                                                                                         | **Description**                                                     |
|--------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------|
| `TaskScheduler(ThreadPool& pool)`                                                                                  | Constuctor that takes a `ThreadPool` object by reference.           |
| `TaskID submit(std::function<void()> task, TimePoint when, int priority = 0,const std::vector<TaskID>& dependencies = {})` | Submits a task to the priority queue and returns a unique `TaskID`. |

### Usage
Bellow is example usage found in [test.cpp](test.cpp) using the `DAGtest()` function, this test has task dependcies structured as:
![Task Dependency DAG](imgs/DAG%20Tasks.png)

```c++
#include "ThreadPool.hpp"
#include "TaskScheduler.hpp"

#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <mutex>

void DAGtest() {} // Defined in test.cpp

int main() {
    DAGtest();
}
```
With an expected output of:
```
A done on thread x
B done on thread x
D done on thread x
C done on thread x
I done on thread x
E done on thread x
F done on thread x
H done on thread x
G done on thread x
J done on thread x
Root 0
Root 1
Root 2
Root 3
Root 4
Test finished
```

****

## General Usage
Above it was only shown how to use for the DAG test, but below is a more simple general usage of both the thread pool and task scheduler.
```c++
#include "ThreadPool.hpp"
#include "TaskScheduler.hpp"

#include <iostream>
#include <chrono>
#include <thread>

int main() {
    using namespace std::chrono_literals;

    // Create thread pool
    ThreadPool pool(4);

    // Attach scheduler to pool
    TaskScheduler scheduler(pool);

    // Submit independent tasks
    auto T1 = scheduler.submit([] {
        std::cout << "Task 1 running\n";
        std::this_thread::sleep_for(200ms);
    });

    auto T2 = scheduler.submit([] {
        std::cout << "Task 2 running\n";
        std::this_thread::sleep_for(150ms);
    });

    // Submit dependent task (runs after T1 and T2)
    auto T3 = scheduler.submit([] {
        std::cout << "Task 3 (dependent) running\n";
    }, Clock::now(), 0, {T1, T2});

    // Allow execution time
    std::this_thread::sleep_for(2s);

    std::cout << "Example finished\n";
}
```
****

## Summary
Overall, this project taught me alot about the C++ Standard Library concurrency primitive types and how to use them effectively.
Some possible future implementations could involve going lock free following a Chase-Lev style implementation of work stealing.
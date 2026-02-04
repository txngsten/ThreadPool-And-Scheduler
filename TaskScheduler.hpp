#ifndef THREADPOOL_TASKSCHEDULER_H
#define THREADPOOL_TASKSCHEDULER_H

#include "ThreadPool.hpp"
#include <chrono>
#include <functional>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <atomic>

// Improves readability.
using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;
using Clock = std::chrono::steady_clock;
using TaskID = std::uint64_t;

class TaskScheduler {
public:
    // Initializes a task scheduler with a thread pool.
    explicit TaskScheduler(ThreadPool& pool);

    // Safely and cleanly shuts down the scheduler and the scheduler thread.
    ~TaskScheduler();

    // Will check dependencies, and if there are none, it will be submitted to the main task queue.
    TaskID submit(std::function<void()> task, TimePoint when, int priority = 0,
            const std::vector<TaskID>& dependencies = {});

private:
    struct ScheduledTask {
        TaskID id;
        TimePoint startTime;
        int priority;
        std::function<void()> fn;
        ScheduledTask(TaskID id, TimePoint startTime, int priority, std::function<void()> fn) :
            id(id), startTime(startTime), priority(priority), fn(std::move(fn)) {}
    };

    // Similar to the thead scheduler loop, the predicate is whether the stop has been triggered or
    // if the queue is NOT empty.
    void schedulerLoop();

    // Will decrease the dependency count for all tasks that were dependent on this task.
    // If any of those dependent tasks have a count of 0, they will be pushed to the taskQueue.
    void onTaskFinish(TaskID id);

    // For priority queue comparison.
    struct TaskCompare {
        const std::unordered_map<TaskID, ScheduledTask>* tasks = nullptr;

        bool operator()(TaskID a, TaskID b) const {
            const auto& ta = tasks->at(a);
            const auto& tb = tasks->at(b);

            if (ta.startTime != tb.startTime)
                return ta.startTime > tb.startTime;
            return ta.priority < tb.priority;
        }
    };

    // Runnable tasks.
    std::priority_queue<TaskID, std::vector<TaskID>, TaskCompare> taskQueue;

    // Tasks waiting on dependencies to finish.
    std::unordered_map<TaskID, ScheduledTask> tasks;

    // Maps TaskID to the number of unfinished tasks each dependency has.
    std::unordered_map<TaskID, int> remainingDeps;

    // Adjacency list for DAG dependencies.
    std::unordered_map<TaskID, std::vector<TaskID>> adjacency;

    // Monotonic counter/incrementer for TaskID's.
    std::atomic<TaskID> nextID {1};

    std::mutex mutex;
    std::condition_variable cv;
    std::thread schedulerThread;

    // Shutdown flag
    bool shutdown {false};
    ThreadPool& pool;
};

#endif //THREADPOOL_TASKSCHEDULER_H
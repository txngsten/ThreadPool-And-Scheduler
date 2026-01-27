#ifndef THREADPOOL_TASKSCHEDULER_H
#define THREADPOOL_TASKSCHEDULER_H

#include "ThreadPool.h"
#include <chrono>
#include <functional>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <atomic>

using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;
using Clock = std::chrono::steady_clock;
using TaskID = std::uint64_t;

class TaskScheduler {
public:
    /**
     * @brief Creates a task scheduler for a given thread pool.
     * Initializes a 'Scheduler' thread which once spawned will call schedulerLoop().
     *
     * @param pool A thread pool object parsed by reference.
     */
    explicit TaskScheduler(ThreadPool& pool);

    /**
     * @brief Safely and cleanly shuts down the task scheduler.
     */
    ~TaskScheduler();

    /**
     *
     */
    TaskID submit(std::function<void()> task, TimePoint when, int priority = 0,
            const std::vector<TaskID>& dependencies = {});

    /**
     *
     */
    TaskID submitAfter(std::function<void()> task, Clock::duration delay, int priority = 0,
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

    /**
     * @brief Scheduler thread will acquire a lock and check if the priority queue has tasks or if the
     * shutdown state is true. If a task is available, it will be parsed to the worker thread pool
     * and popped off the priority queue.
     */
    void schedulerLoop();

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

    // Member variables.
    std::mutex mutex;
    std::condition_variable cv;
    std::thread schedulerThread;
    bool shutdown {false};
    ThreadPool& pool;
};

#endif //THREADPOOL_TASKSCHEDULER_H

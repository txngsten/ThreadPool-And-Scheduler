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

using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;
using Clock = std::chrono::steady_clock;

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
     * @brief Pushes a task to the priority queue with a given start time and priority value.
     *
     * @param task A callable.
     * @param when A timestamp for when the task is to be executed at earliest
     * @param priority An integer value that indicates a tasks priority, second to the time.
     */
    void schedule(std::function<void()> task, TimePoint when, int priority = 0);

    /**
     * @brief Calls the schedule() function and sets when parameter to the current time and delay.
     *
     * @param task A callable.
     * @param delay A time duration to be added to the current time.
     * @param priority n integer value that indicates a tasks priority, second to the time.
     */
    void scheduleAfter(std::function<void()> task, Clock::duration delay, int priority = 0);

private:
    /**
     * @brief Scheduler thread will acquire a lock and check if the priority queue has tasks or if the
     * shutdown state is true. If a task is available, it will be parsed to the worker thread pool
     * and popped off the priority queue.
     */
    void schedulerLoop();

    struct ScheduledTask {
        TimePoint startTime;
        int priority;
        std::function<void()> fn;
        ScheduledTask(TimePoint startTime, int priority, std::function<void()> fn) :
            startTime(startTime), priority(priority), fn(std::move(fn)) {}
    };

    // For priority queue comparison.
    struct TaskCompare {
        bool operator()(const ScheduledTask &task1, const ScheduledTask &task2) const {
            if (task1.startTime != task2.startTime) {
                return task1.startTime > task2.startTime;
            }
            return task1.priority < task2.priority;
        }
    };

    // Member variables.
    std::priority_queue<ScheduledTask, std::vector<ScheduledTask>, TaskCompare> taskQueue;

    std::mutex mutex;
    std::condition_variable cv;
    std::thread schedulerThread;
    bool shutdown {false};
    ThreadPool& pool;
};

#endif //THREADPOOL_TASKSCHEDULER_H

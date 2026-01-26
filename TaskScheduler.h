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
    explicit TaskScheduler(ThreadPool& pool);

    ~TaskScheduler();

    void schedule(std::function<void()> task, TimePoint when, int priority = 0);

    void scheduleAfter(std::function<void()> task, TimePoint::clock delay, int priority = 0);




private:
    struct ScheduledTask {
        std::function<void()> fn;
        int priority;
        TimePoint startTime;
    };

    struct TaskCompare {
        bool operator()(const ScheduledTask &task1, const ScheduledTask &task2) const {
            if (task1.startTime != task2.startTime) {
                return task1.startTime > task2.startTime;
            }
            return task1.priority < task2.priority;
        }
    };

    std::priority_queue<ScheduledTask, std::vector<ScheduledTask>, TaskCompare> taskQueue;

    std::mutex mutex;
    std::condition_variable cv;
    std::thread schedulerThread;
    bool shutdown {false};
    ThreadPool& pool;
};


#endif //THREADPOOL_TASKSCHEDULER_H

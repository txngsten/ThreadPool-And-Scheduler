#include "TaskScheduler.h"
#include "ThreadPool.h"
#include <chrono>
#include <functional>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

TaskScheduler::TaskScheduler(ThreadPool &pool) : pool(pool) {
    schedulerThread = std::thread(&TaskScheduler::schedulerLoop, this);
}

void TaskScheduler::schedulerLoop() {
    std::unique_lock<std::mutex> lock(mutex);

    while (true) {
        cv.wait(lock, [this] {
            return shutdown || !taskQueue.empty();
        });

        if (shutdown) {
            return;
        }

        auto next = taskQueue.top();
        auto now = Clock::now();

        if (now < next.startTime) {
            cv.wait_until(lock, next.startTime);
            continue;
        }

        taskQueue.pop();

        lock.unlock();
        pool.submit(std::move(next.fn));
        lock.lock();
    }
}

void TaskScheduler::schedule(std::function<void()> task, TimePoint when, int priority) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        taskQueue.emplace(when, priority, std::move(task));
    }
    cv.notify_one();
}

void TaskScheduler::scheduleAfter(std::function<void()> task, Clock::duration delay, int priority) {
    schedule(std::move(task), Clock::now() + delay, priority);
}

TaskScheduler::~TaskScheduler() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        shutdown = true;
    }
    cv.notify_one();
    schedulerThread.join();
}
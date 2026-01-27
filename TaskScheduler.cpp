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

        TaskID id = taskQueue.top();
        auto& task = tasks.at(id);

        taskQueue.pop();

        auto fn = std::move(task.fn);

        lock.unlock();
        pool.submit(std::move(fn));
        lock.lock();
    }
}

TaskID TaskScheduler::submit(std::function<void()> task, TimePoint when, int priority,
        const std::vector<TaskID>& dependencies) {
    TaskID id = nextID.fetch_add(1, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(mutex);
        tasks.emplace(id, ScheduledTask {id, when, priority, std::move(task)});

        remainingDeps[id] = static_cast<int>(dependencies.size());
        for (TaskID dep : dependencies) {
            adjacency[dep].push_back(id);
        }

        if (remainingDeps[id] == 0) {
            taskQueue.push(id);
            cv.notify_one();
        }
    }

    return id;
}

TaskID TaskScheduler::submitAfter(std::function<void()> task, Clock::duration delay, int priority,
        const std::vector<TaskID>& dependencies) {
    return submit(std::move(task), Clock::now() + delay, priority, dependencies);
}

TaskScheduler::~TaskScheduler() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        shutdown = true;
    }
    cv.notify_one();
    schedulerThread.join();
}
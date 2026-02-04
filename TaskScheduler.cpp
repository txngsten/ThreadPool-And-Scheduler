#include "TaskScheduler.hpp"
#include "ThreadPool.hpp"

#include <chrono>
#include <functional>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

TaskScheduler::TaskScheduler(ThreadPool &pool) : taskQueue(TaskCompare{&tasks}), pool(pool) {
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
        TaskID finishedID = id;

        // This ensures that only once a task is finished will its dependency count decrement.
        auto wrapped = [this, finishedID, fn = std::move(fn)] () mutable {
            fn();
            onTaskFinish(finishedID);
        };

        lock.unlock();
        pool.submit(std::move(wrapped));
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

void TaskScheduler::onTaskFinish(TaskID id) {
    std::lock_guard<std::mutex> lock(mutex);

    auto it = adjacency.find(id);
    if (it == adjacency.end()) return;

    for (TaskID dep : it->second) {
        int& deps = remainingDeps[dep];
        deps--;

        if (deps == 0) {
            taskQueue.push(dep);
        }
    }
    cv.notify_one();
}

TaskScheduler::~TaskScheduler() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        shutdown = true;
    }
    cv.notify_one();
    schedulerThread.join();
}
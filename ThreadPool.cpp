#include "ThreadPool.hpp"

#include <mutex>
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <condition_variable>

ThreadPool::ThreadPool(size_t numThreads) : deques(), stats(numThreads)  {
    deques.reserve(numThreads);

    for (size_t i {}; i < numThreads; i++) {
        deques.emplace_back(64);
    }

    for (size_t i {}; i < numThreads; i++) {
        workers.emplace_back([this, i] {
            workerLoop(i);
        });
    }
}

void ThreadPool::submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        tasks.push(std::move(task));
    }
    cv.notify_one();
}

void ThreadPool::workerLoop(size_t workerId) {
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [this] {
                return stop || !tasks.empty();
            });

            if (stop && tasks.empty()) {
                return;
            }

            task = std::move(tasks.front());
            tasks.pop();
        }

        task();

        stats[workerId].tasksComplete++;
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        stop = true;
    }

    cv.notify_all();

    for (auto& t : workers) {
        t.join();
    }
}
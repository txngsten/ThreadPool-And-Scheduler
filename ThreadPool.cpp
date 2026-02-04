#include "ThreadPool.hpp"

#include <mutex>
#include <vector>
#include <thread>
#include <functional>
#include <condition_variable>
#include <random>

ThreadPool::ThreadPool(size_t numThreads) : deques(numThreads), stats(numThreads)  {
    for (size_t i {}; i < numThreads; i++) {
        workers.emplace_back([this, i] {
            workerLoop(i);
        });
    }
}

void ThreadPool::submit(std::function<void()> task) {
    size_t idx = nextWorker.fetch_add(1, std::memory_order_relaxed) % deques.size();

    {
        std::lock_guard<std::mutex> lock(deques[idx].workerMutex);
        deques[idx].buffer.push_back(std::move(task));
    }
    cv.notify_one();
}

bool ThreadPool::trySteal(size_t thiefId, std::function<void()>& task) {
    const size_t n = deques.size();
    if (n <= 1) return false;

    // Random generation of victim to ensure no bias.
    thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<size_t> dist(0, n - 1);

    // Can be altered depending on performance needs.
    constexpr int ATTEMPTS = 4;

    for (int i {}; i < ATTEMPTS; i++) {
        size_t victim = dist(rng);
        if (victim == thiefId) continue;

        auto& dq = deques[victim];
        if (dq.workerMutex.try_lock()) {
            std::unique_lock<std::mutex> lock(dq.workerMutex, std::adopt_lock);
            if (!dq.buffer.empty()) {
                task = std::move(dq.buffer.front());
                dq.buffer.pop_front();
                return true;
            }
        }
    }
    return false;
}

void ThreadPool::workerLoop(size_t workerId) {
    while (true) {
        std::function<void()> task;

        {
            auto& dq = deques[workerId];
            std::lock_guard<std::mutex> lock(dq.workerMutex);
            if (!dq.buffer.empty()) {
                task = std::move(dq.buffer.back());
                dq.buffer.pop_back();
            }
        }

        if (task) {
            task();
            stats[workerId].tasksComplete++;
            continue;
        }

        if (trySteal(workerId, task)) {
            task();
            stats[workerId].tasksComplete++;
            continue;
        }

        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] {
            if (stop) return true;
            for (auto& dq : deques) {
                std::lock_guard<std::mutex> g(dq.workerMutex);
                if (!dq.buffer.empty()) return true;
            }
            return false;
        });

        if (stop) {
            return;
        }
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
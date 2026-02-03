#ifndef THREADPOOL_THREADPOOL_H
#define THREADPOOL_THREADPOOL_H

#include <mutex>
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <condition_variable>
#include <cassert>

class ThreadPool {
private:
    // Gets its own cache line
    struct alignas(64) WorkerStats {
        size_t tasksComplete {};
    };

public:
    explicit ThreadPool(size_t numThreads);

    ~ThreadPool();

    void submit(std::function<void()> task);

    [[nodiscard]] std::vector<WorkerStats> getStats() const {
        return stats;
    }

private:
    void workerLoop(size_t workerId);

    struct alignas(64) WorkerDeque {
        std::vector<std::function<void()>> buffer;
        std::atomic<size_t> top;
        size_t bottom;
        size_t mask;

        WorkerDeque(size_t capacity) :
            buffer(capacity),
            top(0),
            bottom(0),
            mask(capacity - 1) {
            // Ensures container size is always in powers of two
            assert(capacity >= 2);
            assert((capacity & (capacity - 1)) == 0);
        }

        // Explicit move constructor
        WorkerDeque(WorkerDeque&& other) noexcept :
            buffer(std::move(other.buffer)),
            top(other.top.load(std::memory_order::relaxed)),
            bottom(other.bottom),
            mask(other.mask) {}

        // Explicit move assignment
        WorkerDeque& operator=(WorkerDeque&& other) noexcept {
            if (this != &other) {
                buffer = std::move(other.buffer);
                top.store(other.top.load(std::memory_order_relaxed),
                    std::memory_order_relaxed);
                bottom = other.bottom;
                mask = other.mask;
            }

            return *this;
        }

        // Delete
        WorkerDeque(const WorkerDeque&) = delete;
        WorkerDeque operator=(const WorkerDeque&) = delete;

    };
    static_assert(alignof(WorkerDeque) >= 64, "WorkerDeque must be cache aligned");
    static_assert(sizeof(WorkerDeque) % 64 == 0, "WorkerDeque size should be a multiple of cache line size");

    // Member variables.
    std::vector<WorkerDeque> deques;
    std::vector<std::thread> workers;
    std::vector<WorkerStats> stats;
    std::queue<std::function<void()>> tasks;

    std::mutex mutex;
    std::condition_variable cv;
    bool stop {false};
};

#endif //THREADPOOL_THREADPOOL_H
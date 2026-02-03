#ifndef THREADPOOL_THREADPOOL_H
#define THREADPOOL_THREADPOOL_H

#include <mutex>
#include <vector>
#include <thread>
#include <functional>
#include <condition_variable>
#include <deque>

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
    bool trySteal(size_t thiefId, std::function<void()>& task);

    void workerLoop(size_t workerId);

    struct alignas(64) WorkerDeque {
        std::deque<std::function<void()>> buffer;
        std::mutex workerMutex;


    };
    static_assert(alignof(WorkerDeque) >= 64, "WorkerDeque must be cache aligned");

    // Member variables.
    std::vector<WorkerDeque> deques;
    std::vector<std::thread> workers;
    std::vector<WorkerStats> stats;

    std::atomic<size_t> nextWorker {0};
    std::mutex mutex;
    std::condition_variable cv;
    bool stop {false};
};

#endif //THREADPOOL_THREADPOOL_H
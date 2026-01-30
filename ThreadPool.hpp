#ifndef THREADPOOL_THREADPOOL_H
#define THREADPOOL_THREADPOOL_H

#include <mutex>
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <condition_variable>

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

    // struct WorkerDeque {
    //     std::vector<std::function<void()>> buffer;
    //     std::atomic<size_t> top;
    //     size_t bottom;
    //     size_t mask;
    //
    //     WorkerDeque(size_t capacity) :
    //         buffer(capacity),
    //         top(0),
    //         bottom(0),
    //         mask(capacity - 1) {
    //         assert(capacity >= 2);
    //         assert((capacity & (capacity - 1)) == 0);
    //     }
    // };

    // Member variables.
    std::vector<std::thread> workers;
    std::vector<WorkerStats> stats;
    std::queue<std::function<void()>> tasks;

    std::mutex mutex;
    std::condition_variable cv;
    bool stop {false};
};

#endif //THREADPOOL_THREADPOOL_H
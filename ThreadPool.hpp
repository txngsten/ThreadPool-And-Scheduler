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
    // Gets its own cache line to prevent false sharing.
    struct alignas(64) WorkerStats {
        size_t tasksComplete {};
    };

public:
    // Creates a thread pool of size numThreads, also assigns unique ID to each worker.
    explicit ThreadPool(size_t numThreads);

    // Safely and cleanly destructs the thread pool.
    ~ThreadPool();

    // Will submit a task to the pool where the atomic nextWorker variable will decide which thread gets it.
    void submit(std::function<void()> task);

    // [[nodiscard]] clang suggested and makes sense since we always want an lvalue for this.
    [[nodiscard]] std::vector<WorkerStats> getStats() const {
        return stats;
    }

private:
    // If a workers deque is empty, the workerLoop will allow the worker to call trySteal()
    // The thief will get 4 attempts to steal before going back to sleep.
    // If a steal is successful, the thief will return and immediately execute the task.
    bool trySteal(size_t thiefId, std::function<void()>& task);

    // Main worker loop where threads will be awoken and sent to.
    // Once a thread is awake in the loop, the stop flag and its own deque will be checked.
    // If the deque is not empty, it will execute its own task.
    // If the deque is empty it will trySteal().
    void workerLoop(size_t workerId);

    // Each thread gets their own deque and is cache aligned to prevent false sharing.
    struct alignas(64) WorkerDeque {
        std::deque<std::function<void()>> buffer;
        std::mutex workerMutex;
    };

    std::vector<WorkerDeque> deques;
    std::vector<std::thread> workers;
    std::vector<WorkerStats> stats;

    // Used for determining which worker gets the next task.
    std::atomic<size_t> nextWorker {0};
    std::mutex mutex;
    std::condition_variable cv;

    // Used as a flag for terminating the thread pool.
    bool stop {false};
};

#endif //THREADPOOL_THREADPOOL_H
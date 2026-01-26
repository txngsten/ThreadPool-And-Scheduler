#ifndef THREADPOOL_THREADPOOL_H
#define THREADPOOL_THREADPOOL_H

#include <mutex>
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <condition_variable>

class ThreadPool {
public:
    /**
     * @brief Creates a threadpool with a given number of worker threads.
     *
     * @param numThreads Number of worker threads.
     */
    explicit ThreadPool(size_t numThreads);

    /**
     * @brief Gracefully shuts down all worker threads.
     */
    ~ThreadPool();

    /**
     * @brief Pushes tasks onto a task queue.
     *
     * @param task any callable type.
     */
    void submit(std::function<void()> task);

private:
    /**
     * @brief Main worker loop executed by each worker thread.
     *
     * Threads acquire the mutex and wait on a condition variable until either
     * a task becomes available or shutdown is requested. When a task is available,
     * the thread removes it from the queue, releases the lock, and executes it.
     */
    void workerLoop();

    // Member variables.
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex mutex;
    std::condition_variable cv;
    bool stop {false};
};

#endif //THREADPOOL_THREADPOOL_H
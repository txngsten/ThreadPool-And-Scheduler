#include "ThreadPool.hpp"
#include "TaskScheduler.hpp"

#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <mutex>

void DAGtest() {
    using namespace std::chrono_literals;

    ThreadPool pool(4);
    TaskScheduler scheduler(pool);

    auto sleepPrint = [](char name, int ms) {
        return [=] {
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            std::cout << name << " done on thread "
                      << std::this_thread::get_id() << "\n";
        };
    };

    // Layer 0
    auto A = scheduler.submit(sleepPrint('A', 200), Clock::now());

    // Layer 1
    auto B = scheduler.submit(sleepPrint('B', 100), Clock::now(), 0, {A});
    auto C = scheduler.submit(sleepPrint('C', 150), Clock::now(), 0, {A});
    auto D = scheduler.submit(sleepPrint('D', 120), Clock::now(), 0, {A});

    // Layer 2
    auto E = scheduler.submit(sleepPrint('E', 80),  Clock::now(), 0, {B});
    auto F = scheduler.submit(sleepPrint('F', 90),  Clock::now(), 0, {B});
    auto G = scheduler.submit(sleepPrint('G', 110), Clock::now(), 0, {C});
    auto H = scheduler.submit(sleepPrint('H', 70),  Clock::now(), 0, {D});
    auto I = scheduler.submit(sleepPrint('I', 60),  Clock::now(), 0, {D});

    // Fan-in sink
    auto J = scheduler.submit(sleepPrint('J', 50), Clock::now(), 0, {E, F, G, H, I});

    // Repeat pattern to increase load
    std::vector<TaskID> sinks;
    sinks.push_back(J);

    for (int i = 0; i < 5; ++i) {
        auto root = scheduler.submit(
            [i] {
                std::cout << "Root " << i << "\n";
            },
            Clock::now(),
            0,
            sinks
        );
        sinks.clear();
        sinks.push_back(root);
    }

    // Keep main alive long enough
    std::this_thread::sleep_for(5s);

    std::cout << "Test finished\n";
}

void WorkStealingTest() {
    using namespace std::chrono_literals;

    constexpr size_t NUM_THREADS = 4;
    constexpr int LONG_TASKS = 24;
    constexpr int SHORT_TASKS = 8;

    ThreadPool pool(NUM_THREADS);
    TaskScheduler scheduler(pool);

    std::mutex coutMutex;

    auto longTask = [&](int id) {
        return [&, id] {
            {
                std::lock_guard<std::mutex> lock(coutMutex);
                std::cout << "[LONG  " << id << "] start on thread "
                          << std::this_thread::get_id() << "\n";
            }

            std::this_thread::sleep_for(300ms);

            {
                std::lock_guard<std::mutex> lock(coutMutex);
                std::cout << "[LONG  " << id << "] end   on thread "
                          << std::this_thread::get_id() << "\n";
            }
        };
    };

    auto shortTask = [&](int id) {
        return [&, id] {
            {
                std::lock_guard<std::mutex> lock(coutMutex);
                std::cout << "[SHORT " << id << "] run   on thread "
                          << std::this_thread::get_id() << "\n";
            }

            std::this_thread::sleep_for(30ms);
        };
    };

    // Force skew — dump long tasks back-to-back
    std::vector<TaskID> roots;
    for (int i = 0; i < LONG_TASKS; ++i) {
        roots.push_back(
            scheduler.submit(longTask(i), Clock::now())
        );
    }

    // Add a few short tasks dependent on nothing
    for (int i = 0; i < SHORT_TASKS; ++i) {
        scheduler.submit(shortTask(i), Clock::now());
    }

    // Give enough time for stealing to fully kick in
    std::this_thread::sleep_for(6s);

    auto stats = pool.getStats();

    std::cout << "\n=== Thread execution stats ===\n";
    for (size_t i = 0; i < stats.size(); ++i) {
        std::cout << "Thread " << i
                  << " executed " << stats[i].tasksComplete
                  << " tasks\n";
    }

    std::cout << "================================\n";
}




int main() {
    DAGtest();
    // WorkStealingTest();
}
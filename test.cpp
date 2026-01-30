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

void mixedDurationTest() {
    using namespace std::chrono_literals;

    constexpr int numThreads = 2;

    ThreadPool pool(numThreads);
    TaskScheduler scheduler(pool);

    auto now = std::chrono::steady_clock::now();

    // Imbalanced workload: 2 long, many short
    scheduler.submit([] { std::this_thread::sleep_for(300ms); }, now);
    scheduler.submit([] { std::this_thread::sleep_for(20ms);  }, now);
    scheduler.submit([] { std::this_thread::sleep_for(20ms);  }, now);
    scheduler.submit([] { std::this_thread::sleep_for(20ms);  }, now);

    scheduler.submit([] { std::this_thread::sleep_for(300ms); }, now);
    scheduler.submit([] { std::this_thread::sleep_for(20ms);  }, now);
    scheduler.submit([] { std::this_thread::sleep_for(20ms);  }, now);
    scheduler.submit([] { std::this_thread::sleep_for(20ms);  }, now);

    // Let all tasks finish (temporary – until wait_all exists)
    std::this_thread::sleep_for(1s);

    // Print stats AFTER execution
    const auto& stats = pool.getStats();
    for (size_t i = 0; i < stats.size(); ++i) {
        std::cout << "Worker " << i
                  << " executed "
                  << stats[i].tasksComplete
                  << " tasks\n";
    }

    std::cout << "Mixed-duration test finished\n";
}


int main() {
    // DAGtest();
    mixedDurationTest();
}
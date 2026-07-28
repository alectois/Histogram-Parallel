#include "histogram-common.hpp"

#include <mutex>
#include <thread>
#include <vector>

class GlobalMutexHistogram {
public:
    explicit GlobalMutexHistogram(std::size_t bucket_count)
        : bins_(bucket_count, 0) {}

    void add(std::size_t value) {
        const std::lock_guard<std::mutex> lock(mutex_);
        ++bins_[value];
    }

    std::vector<std::uint64_t> snapshot() const {
        const std::lock_guard<std::mutex> lock(mutex_);
        return bins_;
    }

private:
    std::vector<std::uint64_t> bins_;
    mutable std::mutex mutex_;
};

int main(int argc, char** argv) {
    return guarded_main([&]() {
        const Config config = parse_args(argc, argv);
        GlobalMutexHistogram histogram(config.max_value + 1);
        std::vector<std::thread> threads;
        threads.reserve(config.num_threads);

        const auto start = Clock::now();
        for (std::size_t thread_id = 0; thread_id < config.num_threads; ++thread_id) {
            const auto work_items =
                partition_size(config.sample_size, config.num_threads, thread_id);
            threads.emplace_back([&, thread_id, work_items]() {
                generate_and_add(
                    work_items,
                    config.max_value,
                    config.seed,
                    thread_id,
                    [&](std::size_t value) { histogram.add(value); });
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }
        const auto end = Clock::now();

        print_result(config, histogram.snapshot(), elapsed_seconds(start, end));
        return EXIT_SUCCESS;
    });
}

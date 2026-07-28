#include "histogram-common.hpp"

#include <mutex>
#include <thread>
#include <vector>

class PerBucketMutexHistogram {
public:
    explicit PerBucketMutexHistogram(std::size_t bucket_count)
        : bins_(bucket_count, 0), mutexes_(bucket_count) {}

    void add(std::size_t value) {
        const std::lock_guard<std::mutex> lock(mutexes_[value]);
        ++bins_[value];
    }

    std::vector<std::uint64_t> snapshot() const {
        std::vector<std::uint64_t> copy(bins_.size());
        for (std::size_t i = 0; i < bins_.size(); ++i) {
            const std::lock_guard<std::mutex> lock(mutexes_[i]);
            copy[i] = bins_[i];
        }
        return copy;
    }

private:
    std::vector<std::uint64_t> bins_;
    mutable std::vector<std::mutex> mutexes_;
};

int main(int argc, char** argv) {
    return guarded_main([&]() {
        const Config config = parse_args(argc, argv);
        PerBucketMutexHistogram histogram(config.max_value + 1);
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

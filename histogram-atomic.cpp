#include "histogram-common.hpp"

#include <atomic>
#include <thread>
#include <vector>

class AtomicHistogram {
public:
    explicit AtomicHistogram(std::size_t bucket_count)
        : bins_(bucket_count) {
        for (auto& bin : bins_) {
            bin.store(0, std::memory_order_relaxed);
        }
    }

    void add(std::size_t value) {
        bins_[value].fetch_add(1, std::memory_order_relaxed);
    }

    std::vector<std::uint64_t> snapshot() const {
        std::vector<std::uint64_t> copy;
        copy.reserve(bins_.size());
        for (const auto& bin : bins_) {
            copy.push_back(bin.load(std::memory_order_relaxed));
        }
        return copy;
    }

private:
    std::vector<std::atomic<std::uint64_t>> bins_;
};

int main(int argc, char** argv) {
    return guarded_main([&]() {
        const Config config = parse_args(argc, argv);
        AtomicHistogram histogram(config.max_value + 1);
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

#include "histogram-common.hpp"

#include <algorithm>
#include <functional>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
    return guarded_main([&]() {
        const Config config = parse_args(argc, argv);
        std::vector<std::vector<std::uint64_t>> local_histograms(
            config.num_threads,
            std::vector<std::uint64_t>(config.max_value + 1, 0));
        std::vector<std::thread> threads;
        threads.reserve(config.num_threads);

        const auto start = Clock::now();
        for (std::size_t thread_id = 0; thread_id < config.num_threads; ++thread_id) {
            const auto work_items =
                partition_size(config.sample_size, config.num_threads, thread_id);
            threads.emplace_back([&, thread_id, work_items]() {
                auto& local = local_histograms[thread_id];
                generate_and_add(
                    work_items,
                    config.max_value,
                    config.seed,
                    thread_id,
                    [&](std::size_t value) { ++local[value]; });
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        std::vector<std::uint64_t> global_histogram(config.max_value + 1, 0);
        for (const auto& local : local_histograms) {
            std::transform(
                local.begin(),
                local.end(),
                global_histogram.begin(),
                global_histogram.begin(),
                std::plus<>());
        }
        const auto end = Clock::now();

        print_result(config, global_histogram, elapsed_seconds(start, end));
        return EXIT_SUCCESS;
    });
}

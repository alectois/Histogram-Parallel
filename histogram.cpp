#include "histogram-common.hpp"

#include <vector>

int main(int argc, char** argv) {
    return guarded_main([&]() {
        const Config config = parse_args(argc, argv);
        if (config.num_threads != 1) {
            throw std::invalid_argument(
                "The sequential baseline requires --num-threads 1");
        }

        std::vector<std::uint64_t> bins(config.max_value + 1, 0);

        const auto start = Clock::now();
        generate_and_add(
            config.sample_size,
            config.max_value,
            config.seed,
            0,
            [&](std::size_t value) { ++bins[value]; });
        const auto end = Clock::now();

        print_result(config, bins, elapsed_seconds(start, end));
        return EXIT_SUCCESS;
    });
}

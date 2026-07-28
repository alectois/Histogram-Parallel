#ifndef HISTOGRAM_COMMON_HPP
#define HISTOGRAM_COMMON_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

struct Config {
    std::size_t max_value = 10;
    std::size_t sample_size = 30'000'000;
    std::size_t num_threads = 1;
    int print_level = 2;
    std::uint64_t seed = 1;
};

inline void print_usage(const char* program) {
    std::cout
        << "Usage: " << program << " [options]\n"
        << "  --num-threads <positive integer>\n"
        << "  --N <non-negative integer>       Maximum generated value (N + 1 buckets)\n"
        << "  --sample-size <positive integer>\n"
        << "  --print-level <0|1|2>            0: time; 1: histogram and time; "
           "2: config, histogram, and time\n"
        << "  --seed <non-negative integer>\n"
        << "  --help\n";
}

inline std::uint64_t parse_unsigned(std::string_view value, std::string_view option) {
    if (value.empty() || value.front() == '-') {
        throw std::invalid_argument("Invalid value for " + std::string(option) + ": " +
                                    std::string(value));
    }

    std::size_t parsed_characters = 0;
    const auto parsed = std::stoull(std::string(value), &parsed_characters);
    if (parsed_characters != value.size()) {
        throw std::invalid_argument("Invalid value for " + std::string(option) + ": " +
                                    std::string(value));
    }
    return parsed;
}

inline std::size_t parse_size(std::string_view value, std::string_view option) {
    const auto parsed = parse_unsigned(value, option);
    if (parsed > std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument("Value for " + std::string(option) +
                                    " exceeds the platform limit");
    }
    return static_cast<std::size_t>(parsed);
}

inline Config parse_args(int argc, char** argv) {
    Config config;

    for (int i = 1; i < argc; ++i) {
        const std::string_view option(argv[i]);
        if (option == "--help") {
            print_usage(argv[0]);
            std::exit(EXIT_SUCCESS);
        }

        const auto next_value = [&]() -> std::string_view {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for " + std::string(option));
            }
            return argv[++i];
        };

        if (option == "--num-threads") {
            config.num_threads = parse_size(next_value(), option);
        } else if (option == "--N") {
            config.max_value = parse_size(next_value(), option);
        } else if (option == "--sample-size") {
            config.sample_size = parse_size(next_value(), option);
        } else if (option == "--print-level") {
            const auto print_level = parse_unsigned(next_value(), option);
            if (print_level > 2) {
                throw std::invalid_argument("--print-level must be 0, 1, or 2");
            }
            config.print_level = static_cast<int>(print_level);
        } else if (option == "--seed") {
            config.seed = parse_unsigned(next_value(), option);
        } else {
            throw std::invalid_argument("Unknown option: " + std::string(option));
        }
    }

    if (config.num_threads == 0) {
        throw std::invalid_argument("--num-threads must be positive");
    }
    if (config.sample_size == 0) {
        throw std::invalid_argument("--sample-size must be positive");
    }
    if (config.max_value == std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument("--N is too large");
    }
    return config;
}

inline std::size_t partition_size(
    std::size_t total,
    std::size_t partitions,
    std::size_t partition_id) {
    const auto base_size = total / partitions;
    const auto remainder = total % partitions;
    return base_size + (partition_id < remainder ? 1 : 0);
}

inline std::uint32_t stream_seed(std::uint64_t base_seed, std::size_t stream_id) {
    // SplitMix64: deterministic, inexpensive mixing for independent RNG streams.
    std::uint64_t value =
        base_seed + 0x9e3779b97f4a7c15ULL * (static_cast<std::uint64_t>(stream_id) + 1);
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    value ^= value >> 31U;
    return static_cast<std::uint32_t>(value);
}

template <typename AddValue>
void generate_and_add(
    std::size_t count,
    std::size_t max_value,
    std::uint64_t base_seed,
    std::size_t stream_id,
    AddValue add_value) {
    std::minstd_rand engine(stream_seed(base_seed, stream_id));
    std::uniform_int_distribution<std::size_t> distribution(0, max_value);

    for (std::size_t i = 0; i < count; ++i) {
        add_value(distribution(engine));
    }
}

using Clock = std::chrono::steady_clock;

inline double elapsed_seconds(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double>(end - start).count();
}

inline void print_result(
    const Config& config,
    const std::vector<std::uint64_t>& bins,
    double seconds) {
    if (config.print_level >= 2) {
        std::cout << "N: " << config.max_value
                  << ", sample size: " << config.sample_size
                  << ", threads: " << config.num_threads
                  << ", items processed: " << config.sample_size
                  << ", seed: " << config.seed << '\n';
    }

    if (config.print_level >= 1) {
        for (std::size_t i = 0; i < bins.size(); ++i) {
            std::cout << i << ':' << bins[i] << '\n';
        }
        const auto total =
            std::accumulate(bins.begin(), bins.end(), std::uint64_t{0});
        std::cout << "total:" << total << '\n';
    }

    std::cout << std::setprecision(9) << seconds << '\n';
}

template <typename MainFunction>
int guarded_main(MainFunction function) {
    try {
        return function();
    } catch (const std::exception& exception) {
        std::cerr << "Error: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }
}

#endif

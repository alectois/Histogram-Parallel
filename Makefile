CXX ?= c++
CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra -Wpedantic -pthread

BUILD_DIR := build
PROGRAMS := histogram histogram-mutex histogram-mutex-per-bucket histogram-atomic histogram-best
TARGETS := $(addprefix $(BUILD_DIR)/,$(PROGRAMS))

.PHONY: all clean test tsan

all: $(TARGETS)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%: %.cpp histogram-common.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

test: all
	@set -eu; \
	for program in $(PROGRAMS); do \
		threads=4; \
		if [ "$$program" = histogram ]; then threads=1; fi; \
		output=$$($(BUILD_DIR)/$$program --N 31 --sample-size 100003 \
			--num-threads $$threads --seed 42 --print-level 1); \
		echo "$$output" | grep -q '^total:100003$$'; \
		echo "$$program: ok"; \
	done

tsan:
	$(MAKE) clean
	$(MAKE) CXXFLAGS="-O1 -g -std=c++17 -Wall -Wextra -Wpedantic -pthread \
		-fsanitize=thread -fno-omit-frame-pointer"

clean:
	rm -rf $(BUILD_DIR)

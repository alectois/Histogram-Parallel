#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "Usage: $0 <executable> [output.csv]" >&2
    exit 2
fi

executable=$1
output=${2:-benchmark-results.csv}
sample_size=${SAMPLE_SIZE:-30000000}
seed=${SEED:-1}
repetitions=${REPETITIONS:-3}
read -r -a thread_counts <<< "${THREAD_COUNTS:-1 2 4 8 16 32}"
read -r -a max_values <<< "${MAX_VALUES:-10 20 40 80 160 320 640 1280 2560}"

if [[ ! -x "$executable" ]]; then
    echo "Executable not found or not executable: $executable" >&2
    exit 2
fi

implementation=$(basename "$executable")
printf 'implementation,max_value,bucket_count,threads,sample_size,seed,repetition,seconds\n' > "$output"

for threads in "${thread_counts[@]}"; do
    if [[ "$implementation" == "histogram" && "$threads" != "1" ]]; then
        continue
    fi

    for max_value in "${max_values[@]}"; do
        for ((repetition = 1; repetition <= repetitions; ++repetition)); do
            command=(
                "$executable"
                --N "$max_value"
                --num-threads "$threads"
                --sample-size "$sample_size"
                --seed "$seed"
                --print-level 0
            )

            if [[ -n "${SLURM_JOB_ID:-}" ]]; then
                seconds=$(
                    srun --exclusive --nodes=1 --ntasks=1 --cpus-per-task="$threads" \
                        "${command[@]}"
                )
            else
                seconds=$("${command[@]}")
            fi

            printf '%s,%s,%s,%s,%s,%s,%s,%s\n' \
                "$implementation" "$max_value" "$((max_value + 1))" "$threads" \
                "$sample_size" "$seed" "$repetition" "$seconds" >> "$output"
        done
    done
done

echo "Wrote $output"

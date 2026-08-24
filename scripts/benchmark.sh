#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
    echo "Usage: $0 <radish|redis> <volatile|durable> <output-directory>" >&2
    exit 2
fi

server_kind="$1"
mode="$2"
output_directory="$3"
build_directory="${BUILD_DIRECTORY:-build}"
benchmark_binary="${BENCHMARK_BINARY:-$build_directory/radish-bench}"
radish_server="${RADISH_SERVER:-$build_directory/radish-server}"
redis_server="${REDIS_SERVER:-redis-server}"
port="${BENCHMARK_PORT:-6381}"
repetitions="${BENCHMARK_REPETITIONS:-3}"
warmup="${BENCHMARK_WARMUP_SECONDS:-30}"
duration="${BENCHMARK_DURATION_SECONDS:-60}"
clients="${BENCHMARK_CLIENTS:-1 4 16}"
pipelines="${BENCHMARK_PIPELINES:-1 16}"
workloads="${BENCHMARK_WORKLOADS:-PING GET_HIT SET MIX50 READ95 WRITE95 EXISTS DBSIZE}"
key_count="${BENCHMARK_KEY_COUNT:-100000}"
key_size="${BENCHMARK_KEY_SIZE:-32}"
value_size="${BENCHMARK_VALUE_SIZE:-128}"

if [[ "$server_kind" != "radish" && "$server_kind" != "redis" ]]; then
    echo "Server must be radish or redis" >&2
    exit 2
fi
if [[ "$mode" != "volatile" && "$mode" != "durable" ]]; then
    echo "Mode must be volatile or durable" >&2
    exit 2
fi

cmake --build "$build_directory" --target radish-server radish-bench

mkdir -p "$output_directory"
temporary_directory="$(mktemp -d)"
server_pid=""

cleanup() {
    if [[ -n "$server_pid" ]]; then
        kill "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi
    rm -rf "$temporary_directory"
}
trap cleanup EXIT

if [[ "$server_kind" == "radish" ]]; then
    persistence="off"
    if [[ "$mode" == "durable" ]]; then
        persistence="flush"
    fi
    "$radish_server" --port "$port" --data-file "$temporary_directory/radish" --persistence "$persistence" \
        >"$output_directory/server.log" 2>&1 &
else
    appendonly="no"
    appendfsync="no"
    if [[ "$mode" == "durable" ]]; then
        appendonly="yes"
        appendfsync="${REDIS_APPEND_FSYNC:-no}"
    fi
    "$redis_server" --port "$port" --bind 127.0.0.1 --save "" --appendonly "$appendonly" \
        --appendfsync "$appendfsync" --dir "$temporary_directory" \
        >"$output_directory/server.log" 2>&1 &
fi
server_pid="$!"
ready=false
for attempt in {1..20}; do
    if "$benchmark_binary" --port "$port" --workload PING --duration 1 --warmup 0 --key-count 1 \
        --key-size 8 --value-size 1 --preload off --output "$temporary_directory/readiness" >/dev/null 2>&1; then
        ready=true
        break
    fi
    sleep 1
done
if [[ "$ready" != "true" ]]; then
    echo "Server failed readiness probe; see $output_directory/server.log" >&2
    exit 1
fi

for workload in $workloads; do
    for client_count in $clients; do
        for pipeline_depth in $pipelines; do
            for run in $(seq 1 "$repetitions"); do
                prefix="$output_directory/${server_kind}-${mode}-${workload}-c${client_count}-p${pipeline_depth}-run${run}"
                "$benchmark_binary" --port "$port" --workload "$workload" --clients "$client_count" \
                    --pipeline "$pipeline_depth" --duration "$duration" --warmup "$warmup" \
                    --key-count "$key_count" --key-size "$key_size" --value-size "$value_size" \
                    --seed "$run" --label "${server_kind}-${mode}" --output "$prefix"
            done
        done
    done
done

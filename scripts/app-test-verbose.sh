#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cmake --build "$script_dir/../cmake-build-debug" --target app-test-verbose

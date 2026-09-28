#!/usr/bin/env bash
# Build and run the sample browser. Usage: ./run.sh [Debug|Release] [app args...]
# Set APP=triangle or APP=cube to run a minimal app instead.
set -e
cd "$(dirname "$0")"
TYPE=${1:-Debug}
shift || true
./build.sh "$TYPE"
./build/$TYPE/bin/${APP:-glint_samples} "$@"

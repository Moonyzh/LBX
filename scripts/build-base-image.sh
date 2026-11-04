#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
docker build -f docker/Dockerfile.base -t course/trpc-cpp-dev:2026.09 .

#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../docker"
# Recreate clears the intentional in-memory teaching repository so verification
# can use predictable trip IDs on every fresh run.
docker compose up --build --force-recreate -d pricing trip
docker compose ps

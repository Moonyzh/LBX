#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../docker"
docker compose --profile tools run --rm client /app/ride_client --client_config=/app/config/ride-client.yaml "$@"

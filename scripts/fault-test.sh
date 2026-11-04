#!/usr/bin/env bash
set -euo pipefail

ROOT="$(dirname "$0")"
COMPOSE_DIR="$ROOT/../docker"

expect_failure() {
  local label="$1"
  shift
  set +e
  "$@"
  local status=$?
  set -e
  if [[ $status -eq 0 ]]; then
    echo "ASSERTION FAILED: $label unexpectedly succeeded" >&2
    return 1
  fi
  echo "PASS: $label failed as expected (exit_code=$status)"
}

echo "Stopping pricing-service. Existing orders are still owned by trip-service."
(cd "$COMPOSE_DIR" && docker compose stop pricing)

existing_order="$(bash "$ROOT/client.sh" --op=get --trip_id=trip-000001)"
echo "$existing_order"
if ! grep -Fq "found=1" <<<"$existing_order"; then
  echo "ASSERTION FAILED: an existing order should remain queryable while pricing is down" >&2
  exit 1
fi
echo "PASS: existing order remains queryable while pricing is down"

expect_failure "new order requires pricing-service" \
  bash "$ROOT/client.sh" --op=create --passenger_id=bob --origin=campus --destination=airport --distance_km=25 --time_slot=peak

(cd "$COMPOSE_DIR" && docker compose start pricing)

echo "Stopping trip-service. The client has no order API without it."
(cd "$COMPOSE_DIR" && docker compose stop trip)
expect_failure "GetTrip requires trip-service" bash "$ROOT/client.sh" --op=get --trip_id=trip-000001

(cd "$COMPOSE_DIR" && docker compose start trip)

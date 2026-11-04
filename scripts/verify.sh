#!/usr/bin/env bash
set -euo pipefail

ROOT="$(dirname "$0")"

assert_contains() {
  local text="$1"
  local expected="$2"
  local label="$3"
  if ! grep -Fq "$expected" <<<"$text"; then
    echo "ASSERTION FAILED: $label; expected [$expected]" >&2
    echo "actual output:" >&2
    echo "$text" >&2
    return 1
  fi
  echo "PASS: $label"
}

# scripts/up.sh recreates trip-service, so these IDs and fares are deterministic.
peak_create="$(bash "$ROOT/client.sh" --op=create --passenger_id=alice --origin=campus --destination=railway-station --distance_km=8.5 --time_slot=peak)"
echo "$peak_create"
assert_contains "$peak_create" "create_ok=1" "peak CreateRide succeeds"
assert_contains "$peak_create" "trip_id=trip-000001" "first trip has predictable ID"
assert_contains "$peak_create" "estimated_fare=41.04" "peak fare follows the teaching rule"
assert_contains "$peak_create" "status=WAITING" "new trip starts waiting"

peak_get="$(bash "$ROOT/client.sh" --op=get --trip_id=trip-000001)"
echo "$peak_get"
assert_contains "$peak_get" "found=1" "first trip can be queried"
assert_contains "$peak_get" "estimated_fare=41.04" "queried peak fare is retained"

offpeak_create="$(bash "$ROOT/client.sh" --op=create --passenger_id=alice --origin=library --destination=hospital --distance_km=3.0 --time_slot=offpeak)"
echo "$offpeak_create"
assert_contains "$offpeak_create" "create_ok=1" "offpeak CreateRide succeeds"
assert_contains "$offpeak_create" "trip_id=trip-000002" "second trip has predictable ID"
assert_contains "$offpeak_create" "estimated_fare=17.20" "offpeak fare follows the teaching rule"

offpeak_get="$(bash "$ROOT/client.sh" --op=get --trip_id=trip-000002)"
echo "$offpeak_get"
assert_contains "$offpeak_get" "found=1" "second trip can be queried"
assert_contains "$offpeak_get" "status=WAITING" "second trip retains waiting status"

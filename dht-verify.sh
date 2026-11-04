#!/usr/bin/env bash
# End-to-end check for Lab 1B, run on machine A after all three nodes are up.
#
# Proves two things at once:
#   1. ride_client -> trip-service contract is unchanged (same fares as 1A)
#   2. every order really goes through Put("trip:"+id) / Get("trip:"+id) on the
#      node-a entry, and the ring answers with an owner + hop count
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

failures=0

assert_contains() {
  local text="$1" expected="$2" label="$3"
  if grep -Fq "$expected" <<<"$text"; then
    echo "PASS: $label"
  else
    echo "FAIL: $label (expected [$expected])" >&2
    echo "$text" >&2
    failures=$((failures + 1))
  fi
}

echo "== 1. 创建三笔订单（trip-service 应写入 DHT）=="
create_1="$(bash scripts/client.sh --op=create --passenger_id=alice --origin=campus \
  --destination=railway-station --distance_km=8.5 --time_slot=peak)"
echo "$create_1"
assert_contains "$create_1" "create_ok=1" "peak CreateRide 成功"
assert_contains "$create_1" "trip_id=trip-000001" "第一笔 trip_id 可预测"
assert_contains "$create_1" "estimated_fare=41.04" "peak 票价与 1A 一致"

create_2="$(bash scripts/client.sh --op=create --passenger_id=bob --origin=library \
  --destination=hospital --distance_km=3.0 --time_slot=offpeak)"
echo "$create_2"
assert_contains "$create_2" "create_ok=1" "offpeak CreateRide 成功"
assert_contains "$create_2" "trip_id=trip-000002" "第二笔 trip_id 可预测"
assert_contains "$create_2" "estimated_fare=17.20" "offpeak 票价与 1A 一致"

create_3="$(bash scripts/client.sh --op=create --passenger_id=carol --origin=hotel \
  --destination=airport --distance_km=6.0 --time_slot=peak)"
echo "$create_3"
assert_contains "$create_3" "create_ok=1" "第三笔 CreateRide 成功"
assert_contains "$create_3" "trip_id=trip-000003" "第三笔 trip_id 可预测"

echo
echo "== 2. 按 trip_id 回读（trip-service 应从 DHT 取回并反序列化）=="
for id in trip-000001 trip-000002 trip-000003; do
  got="$(bash scripts/client.sh --op=get --trip_id="$id")"
  echo "$got"
  assert_contains "$got" "found=1" "$id 能从 DHT 读回"
  assert_contains "$got" "status=WAITING" "$id 状态字段保留"
done

missing="$(bash scripts/client.sh --op=get --trip_id=trip-999999)"
echo "$missing"
assert_contains "$missing" "found=0" "不存在的订单返回 found=0"

echo
echo "== 3. 三笔订单确实经过 DHT 入口 node-a =="
log="$(cd docker && docker compose exec -T trip sh -c 'cat /tmp/trip-service.log' 2>/dev/null | grep -F "trip:" || true)"
echo "$log"
if [[ -z "$log" ]]; then
  echo "FAIL: trip-service 日志里没有 DHT 记录" >&2
  failures=$((failures + 1))
else
  for id in trip-000001 trip-000002 trip-000003; do
    line="$(grep -F "dht_key=trip:${id}" <<<"$log" | tail -1 || true)"
    if [[ -n "$line" ]]; then
      echo "PASS: ${id} 的 DHT 记录 -> $(grep -oE 'dht_owner=[^ ]+|dht_hops=[^ ]+' <<<"$line" | tr '\n' ' ')"
    else
      echo "FAIL: 日志里找不到 ${id} 的 DHT 记录" >&2
      failures=$((failures + 1))
    fi
  done
  owners="$(grep -oE 'dht_owner=node-[abc]' <<<"$log" | sort -u | tr '\n' ' ')"
  echo "本次涉及的 owner：${owners:-无}"
fi

echo
if [[ "$failures" -eq 0 ]]; then
  echo "dht 三节点验证：全部通过"
  echo "取证命令： docker compose logs dht-node-a ; docker compose exec -T trip cat /tmp/trip-service.log"
else
  echo "dht 三节点验证：${failures} 项失败" >&2
  exit 1
fi

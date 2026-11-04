#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT}/build}"
SERVER="${BUILD_DIR}/dht_server"
CLIENT="${BUILD_DIR}/dht_smoke_client"
LOG_DIR="${LOG_DIR:-/tmp/lab1b-dht-local}"

if [[ ! -x "${SERVER}" || ! -x "${CLIENT}" ]]; then
  echo "missing build outputs: ${SERVER} or ${CLIENT}" >&2
  exit 2
fi

mkdir -p "${LOG_DIR}"
declare -a pids=()

cleanup() {
  if (( ${#pids[@]} > 0 )); then
    for pid in "${pids[@]}"; do
      kill "${pid}" 2>/dev/null || true
    done
  fi
  wait 2>/dev/null || true
}
trap cleanup EXIT INT TERM

start_node() {
  local name="$1"
  local config="$2"
  local node_id="$3"
  local predecessor_id="$4"
  local successor_service="$5"

  "${SERVER}" \
    --config="${ROOT}/${config}" \
    --node_name="${name}" \
    --node_id="${node_id}" \
    --predecessor_id="${predecessor_id}" \
    --successor_service="${successor_service}" \
    >"${LOG_DIR}/${name}.log" 2>&1 &
  pids+=("$!")
}

wait_port() {
  local port="$1"
  for _ in $(seq 1 100); do
    if (exec 3<>"/dev/tcp/127.0.0.1/${port}") 2>/dev/null; then
      exec 3>&-
      return 0
    fi
    sleep 0.1
  done
  echo "node on port ${port} did not start" >&2
  return 1
}

run_put_get() {
  local key="$1"
  local owner="$2"
  local entry="$3"
  local expected_hops="$4"
  local entry_service="trpc.dht.${entry}.DhtService"
  local output

  echo "CASE key=${key} owner=${owner} entry=${entry} expected_hops=${expected_hops}"

  output="$("${CLIENT}" \
    --client_config="${ROOT}/config/dht-smoke-client.local.yaml" \
    --entry="${entry_service}" \
    --op=put \
    --key="${key}" \
    --value="value-${key}" 2>&1 || true)"
  echo "${output}"
  grep -q "ok=1" <<<"${output}"
  grep -q "owner=${owner}" <<<"${output}"
  grep -q "hops=${expected_hops}" <<<"${output}"

  output="$("${CLIENT}" \
    --client_config="${ROOT}/config/dht-smoke-client.local.yaml" \
    --entry="${entry_service}" \
    --op=get \
    --key="${key}" 2>&1 || true)"
  echo "${output}"
  grep -q "found=1" <<<"${output}"
  grep -q "owner=${owner}" <<<"${output}"
  grep -q "hops=${expected_hops}" <<<"${output}"
  local expected_hex
  expected_hex="$(printf '%s' "value-${key}" | od -An -tx1 | tr -d ' \n')"
  grep -q "value_hex=${expected_hex}" <<<"${output}"
}

start_node node-a config/dht-node-a.local.yaml \
  0x10000000 0xB0000000 trpc.dht.node-b.DhtService
start_node node-b config/dht-node-b.local.yaml \
  0x60000000 0x10000000 trpc.dht.node-c.DhtService
start_node node-c config/dht-node-c.local.yaml \
  0xB0000000 0x60000000 trpc.dht.node-a.DhtService

wait_port 19001
wait_port 19002
wait_port 19003

run_put_get key-0408 node-a node-b 2
run_put_get key-0480 node-a node-c 1
run_put_get key-0020 node-b node-c 2
run_put_get key-0022 node-b node-a 1
run_put_get key-0103 node-c node-a 2
run_put_get key-0105 node-c node-b 1
run_put_get key-0408 node-a node-a 0

hop_limit_output="$("${CLIENT}" \
  --client_config="${ROOT}/config/dht-smoke-client.local.yaml" \
  --entry=trpc.dht.node-a.DhtService \
  --op=get \
  --key=key-0020 \
  --hops=3 || true)"
echo "${hop_limit_output}"
grep -q "found=0" <<<"${hop_limit_output}"
grep -q "hop limit" <<<"${hop_limit_output}"

echo "dht local three-node smoke test: all checks passed"
echo "node logs: ${LOG_DIR}"

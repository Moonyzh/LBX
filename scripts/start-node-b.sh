#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONFIG="${DHT_CONFIG:-${ROOT}/config/dht-node-b.cvm.yaml}"
IMAGE="${DHT_IMAGE:-course/trpc-cpp-dev:2026.09}"
SERVER="${ROOT}/build/dht_server"

if [[ ! -x "${SERVER}" ]]; then
  echo "missing DHT server binary: ${SERVER}" >&2
  exit 2
fi
if [[ ! -f "${CONFIG}" ]]; then
  echo "missing DHT node config: ${CONFIG}" >&2
  exit 2
fi
if grep -q "<NODE_C_REACHABLE_IP>" "${CONFIG}"; then
  echo "replace <NODE_C_REACHABLE_IP> in ${CONFIG} first" >&2
  exit 2
fi

docker rm -f dht-node-b >/dev/null 2>&1 || true

docker run -d \
  --name dht-node-b \
  --restart unless-stopped \
  -p 19001:19001 \
  -v "${SERVER}:/workspace/build/dht_server:ro" \
  -v "${CONFIG}:/workspace/config/dht-node-b.cvm.yaml:ro" \
  -w /workspace \
  "${IMAGE}" \
  /workspace/build/dht_server \
  --config=/workspace/config/dht-node-b.cvm.yaml \
  --node_name=node-b \
  --node_id=0x60000000 \
  --predecessor_id=0x10000000 \
  --successor_service=trpc.dht.node-c.DhtService

docker ps --filter name=dht-node-b

#!/usr/bin/env bash
# Start exactly one DHT node on this machine, plus pricing and trip on machine A.
#
#   machine A: bash scripts/dht-up-node.sh a     # dht-node-a + pricing + trip
#   machine B: bash scripts/dht-up-node.sh b     # dht-node-b only
#   machine C: bash scripts/dht-up-node.sh c     # dht-node-c only
#
# Requires the three reachable addresses: export NODE_A_IP / NODE_B_IP / NODE_C_IP
# (or keep them in node-inventory.env).
set -euo pipefail

NODE="${1:-}"
case "$NODE" in
  a|b|c) ;;
  *)
    echo "usage: bash scripts/dht-up-node.sh a|b|c" >&2
    exit 2
    ;;
esac

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

IMAGE="lab1-ride-microservices:local"

# 1. Render this machine's node config (re-renders all three, harmless).
bash scripts/dht-gen-config.sh

for f in pricing-service trip-service; do
  [[ -f "config/${f}.yaml" ]] || { echo "missing config/${f}.yaml" >&2; exit 2; }
done

# 2. Every machine needs the shared image locally: B and C only run dht_server,
#    but they still start it from the same image.
if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
  echo "image ${IMAGE} not found locally, building it (first run takes a few minutes)"
  (cd docker && docker compose build pricing)
fi

# 3. Start only this machine's services.
cd docker
if [[ "$NODE" == "a" ]]; then
  # trip-service enters the ring at node-a, so pricing and trip live here too.
  docker compose up -d dht-node-a pricing trip
else
  docker compose up -d "dht-node-${NODE}"
fi

sleep 2
docker compose ps

echo
echo "successor target in this machine's node config:"
grep "target:" "${ROOT}/config/dht-node-${NODE}.cvm.yaml" || true

cat <<EOF

下一步：
  * 三台都启动后，在机器 A 上执行  bash scripts/dht-verify.sh
  * 查看本机节点日志        docker compose logs -f dht-node-${NODE}
EOF

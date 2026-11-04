#!/usr/bin/env bash
# Render config/dht-node-{a,b,c}.cvm.yaml from config/dht-node.cvm.template.yaml.
#
# Ring (fixed by the team, matches the --node_id flags in docker-compose.yml):
#   node-a 0x10000000  predecessor 0xB0000000 (node-c)  successor node-b
#   node-b 0x60000000  predecessor 0x10000000 (node-a)  successor node-c
#   node-c 0xB0000000  predecessor 0x60000000 (node-b)  successor node-a
#
# Addresses come from NODE_A_IP / NODE_B_IP / NODE_C_IP, or from node-inventory.env
# (copy node-inventory.example.env and fill NODE_A_HOST / NODE_B_HOST / NODE_C_HOST).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

TEMPLATE="config/dht-node.cvm.template.yaml"
INVENTORY="${NODE_INVENTORY:-node-inventory.env}"

if [[ ! -f "$TEMPLATE" ]]; then
  echo "missing template: $TEMPLATE" >&2
  exit 2
fi

# env vars win; otherwise load the local inventory file
if [[ -z "${NODE_A_IP:-}" || -z "${NODE_B_IP:-}" || -z "${NODE_C_IP:-}" ]]; then
  if [[ -f "$INVENTORY" ]]; then
    # shellcheck disable=SC1090
    set -a && . "$INVENTORY" && set +a
    NODE_A_IP="${NODE_A_IP:-${NODE_A_HOST:-}}"
    NODE_B_IP="${NODE_B_IP:-${NODE_B_HOST:-}}"
    NODE_C_IP="${NODE_C_IP:-${NODE_C_HOST:-}}"
  fi
fi

missing=0
for name in NODE_A_IP NODE_B_IP NODE_C_IP; do
  value="${!name:-}"
  if [[ -z "$value" || "$value" == "<"*">" ]]; then
    echo "需要 $name（三台机器的互通地址）；可直接 export，或写进 $INVENTORY" >&2
    missing=1
  fi
done
[ "$missing" -eq 0 ] || exit 2

render() {
  local letter="$1" node="$2" admin_port="$3" successor="$4" successor_host="$5"
  local out="config/dht-node-${letter}.cvm.yaml"

  sed -e "s/__NODE_NAME__/${node}/g" \
      -e "s/__ADMIN_PORT__/${admin_port}/g" \
      -e "s/__SUCCESSOR_NAME__/${successor}/g" \
      -e "s/__SUCCESSOR_HOST__/${successor_host}/g" \
      "$TEMPLATE" > "$out"

  if grep -q "__" "$out"; then
    echo "占位符未替换干净: $out" >&2
    exit 3
  fi
  echo "generated ${out}  node=${node}  successor=trpc.dht.${successor}.DhtService -> ${successor_host}:19001"
}

render a node-a 18081 node-b "$NODE_B_IP"
render b node-b 18082 node-c "$NODE_C_IP"
render c node-c 18083 node-a "$NODE_A_IP"

echo
echo "ring: node-a(0x10000000) -> node-b(0x60000000) -> node-c(0xB0000000) -> node-a"
echo "三台机器各自的互通地址：A=${NODE_A_IP} B=${NODE_B_IP} C=${NODE_C_IP}"

#!/usr/bin/env bash
# Collect the evidence required by task 8 from all three machines.
# Run on your own computer or on machine A; it only needs SSH access.
#
#   export NODE_A_SSH=ubuntu@<A-IP> NODE_B_SSH=ubuntu@<B-IP> NODE_C_SSH=ubuntu@<C-IP>
#   export REMOTE_DIR=~/lab1-mini-dht
#   bash scripts/dht-collect-evidence.sh
#
# Output: data/evidence/lab1b/node-{a,b,c}/  (hostname, compose ps, node config, logs)
# IPv4 addresses in the collected configs are masked to <IP> so the bundle can be
# attached to the report without leaking the VPC layout.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

REMOTE_DIR="${REMOTE_DIR:-~/lab1-mini-dht}"
OUT_ROOT="${OUT_ROOT:-data/evidence/lab1b}"
STAMP="$(date +%Y%m%d-%H%M%S)"

for name in NODE_A_SSH NODE_B_SSH NODE_C_SSH; do
  if [[ -z "${!name:-}" ]]; then
    echo "需要 $name（形如 ubuntu@1.2.3.4）" >&2
    exit 2
  fi
done

mkdir -p "$OUT_ROOT"

mask_ips() {
  sed -E 's/([0-9]{1,3}\.){3}[0-9]{1,3}/<IP>/g'
}

collect_one() {
  local letter="$1" target="$2"
  local dir="${OUT_ROOT}/node-${letter}"
  mkdir -p "$dir"

  echo "== node-${letter} (${target}) =="

  ssh -o ConnectTimeout=10 "$target" "hostname" > "${dir}/hostname.txt" 2>&1 || true
  ssh -o ConnectTimeout=10 "$target" "cd ${REMOTE_DIR}/docker && docker compose ps" \
    > "${dir}/compose-ps.txt" 2>&1 || true
  ssh -o ConnectTimeout=10 "$target" "cat ${REMOTE_DIR}/config/dht-node-${letter}.cvm.yaml" \
    2>/dev/null | mask_ips > "${dir}/dht-node-${letter}.cvm.masked.yaml" || true
  ssh -o ConnectTimeout=10 "$target" \
    "cd ${REMOTE_DIR}/docker && docker compose logs --no-color --tail=300 dht-node-${letter}" \
    > "${dir}/dht-node-${letter}.log" 2>&1 || true

  # machine A also runs pricing + trip, so keep their logs with A's node
  if [[ "$letter" == "a" ]]; then
    ssh -o ConnectTimeout=10 "$target" \
      "cd ${REMOTE_DIR}/docker && docker compose logs --no-color --tail=300 trip" \
      > "${dir}/trip.log" 2>&1 || true
    ssh -o ConnectTimeout=10 "$target" \
      "cd ${REMOTE_DIR}/docker && docker compose logs --no-color --tail=200 pricing" \
      > "${dir}/pricing.log" 2>&1 || true
    ssh -o ConnectTimeout=10 "$target" \
      "cd ${REMOTE_DIR}/docker && docker compose exec -T trip cat /tmp/trip-service.log" \
      > "${dir}/trip-service.log" 2>&1 || true
  fi

  echo "   -> ${dir}"
}

collect_one a "$NODE_A_SSH"
collect_one b "$NODE_B_SSH"
collect_one c "$NODE_C_SSH"

cat > "${OUT_ROOT}/README-${STAMP}.md" <<EOF
# 实验 1B 取证（${STAMP}）

| 文件 | 内容 |
|---|---|
| \`node-*/hostname.txt\` | 每台机器的主机名 |
| \`node-*/compose-ps.txt\` | 每台机器 \`docker compose ps\`，证明只跑自己的节点 |
| \`node-*/dht-node-*.cvm.masked.yaml\` | 生效的节点配置（IP 已脱敏） |
| \`node-*/dht-node-*.log\` | 节点日志，含 \`put/get key=... owner=... hops=...\` |
| \`node-a/trip.log\`、\`node-a/trip-service.log\` | 订单服务日志，含 \`dht_key/dht_owner/dht_hops\` |
| \`node-a/pricing.log\` | 计价服务日志 |

注意：DHT 节点存储是进程内 map，容器重启即清空。
EOF

echo
echo "evidence: ${OUT_ROOT} (manifest: README-${STAMP}.md)"
find "$OUT_ROOT" -type f -newermt '-5 minutes' | sort | sed 's/^/  /'

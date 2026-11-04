#!/usr/bin/env bash
# Pack the repository for upload to a CVM when there is no reachable git remote.
#
#   bash scripts/pack-repo.sh
#   -> dist/lab1-mini-dht-src.tar.gz   (source only, no build output, no real IPs)
#
# Excluded on purpose: build/, dist/, dht-local-logs/, data/evidence/,
# config/*.cvm.yaml and node-inventory.env (they carry real machine addresses).
set -euo pipefail

cd "$(dirname "$0")/.."

OUT="dist/lab1-mini-dht-src.tar.gz"
mkdir -p dist

tar -czf "$OUT" \
  --exclude=./.git \
  --exclude=./build \
  --exclude=./dht-local-logs \
  --exclude=./data/evidence \
  --exclude=./dist \
  --exclude=./node-inventory.env \
  --exclude='./config/*.cvm.yaml' \
  --exclude=./proto/*.pb.cc \
  --exclude=./proto/*.pb.h \
  .

echo "archive : ${OUT}"
echo -n "size    : "; du -h "$OUT" | cut -f1
echo "files   : $(tar -tzf "$OUT" | grep -vc '/$')"

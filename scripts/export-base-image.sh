#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="${1:-$ROOT/dist}"
IMAGE="course/trpc-cpp-dev:2026.09"
ARCHIVE="$OUT_DIR/trpc-cpp-dev-2026.09.tar.gz"

mkdir -p "$OUT_DIR"
docker image inspect "$IMAGE" >/dev/null
docker image save "$IMAGE" | gzip -1 > "$ARCHIVE"
(cd "$OUT_DIR" && sha256sum "$(basename "$ARCHIVE")" > "$(basename "$ARCHIVE").sha256")
ls -lh "$ARCHIVE" "$ARCHIVE.sha256"
echo "Students import with: docker image load --input $(basename "$ARCHIVE")"

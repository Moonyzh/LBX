#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
COMPOSE_DIR="$ROOT/docker"
SUBMISSION_DIR="$ROOT/submission"
STAMP="$(date +%Y%m%d-%H%M%S)"
RESULT="$SUBMISSION_DIR/lab1a-$STAMP.txt"
JSON_RESULT="$SUBMISSION_DIR/lab1a-$STAMP.result.json"
ARCHIVE="$SUBMISSION_DIR/lab1a-$STAMP.tar.gz"
mkdir -p "$SUBMISSION_DIR"

cleanup() {
  (cd "$COMPOSE_DIR" && docker compose up -d pricing trip) >/dev/null 2>&1 || true
}
trap cleanup EXIT

bash "$ROOT/scripts/up.sh"
normal_ok=false
fault_ok=false
{
  echo "# Lab 1A evidence bundle: ride microservices"
  echo "generated_at=$(date -Is)"
  echo "## Container status"
  (cd "$COMPOSE_DIR" && docker compose ps)
  echo "## Deterministic normal workflow"
  if bash "$ROOT/scripts/verify.sh"; then
    normal_ok=true
    echo "normal_workflow=PASS"
  else
    echo "normal_workflow=FAIL"
  fi
  echo "## Deterministic dependency-failure workflow"
  if bash "$ROOT/scripts/fault-test.sh"; then
    fault_ok=true
    echo "fault_workflow=PASS"
  else
    echo "fault_workflow=FAIL"
  fi
  echo "## Service logs"
  (cd "$COMPOSE_DIR" && docker compose logs --no-color trip pricing)
} > "$RESULT"
cat "$RESULT"

read -r -p "team_id: " TEAM_ID
read -r -p "members (name/student-id, comma separated): " MEMBERS
read -r -p "platform (WSL or Ubuntu): " PLATFORM
read -r -p "AI help used: " AI_HELP
read -r -p "human changes and verification: " HUMAN_WORK
{
  echo "team_id=$TEAM_ID"
  echo "members=$MEMBERS"
  echo "platform=$PLATFORM"
  echo "ai_help=$AI_HELP"
  echo "human_changes_and_verification=$HUMAN_WORK"
} >> "$RESULT"

cat > "$JSON_RESULT" <<EOF
{
  "lab": "lab1a-ride-microservices",
  "normal_workflow_passed": $normal_ok,
  "fault_workflow_passed": $fault_ok,
  "expected_peak_fare": 41.04,
  "expected_offpeak_fare": 17.20
}
EOF

shopt -s nullglob
REPORT_ASSETS=(image*.png image*.jpg image*.jpeg report-images)
shopt -u nullglob

tar -C "$ROOT" -czf "$ARCHIVE" \
  "submission/$(basename "$RESULT")" \
  "submission/$(basename "$JSON_RESULT")" \
  src proto config docker scripts data CMakeLists.txt README.md report-template.md \
  "${REPORT_ASSETS[@]}"
echo "Created: $ARCHIVE"
echo "Machine-readable result: $JSON_RESULT"

if [[ "$normal_ok" != true || "$fault_ok" != true ]]; then
  echo "Evidence package was created, but automated checks failed." >&2
  exit 1
fi

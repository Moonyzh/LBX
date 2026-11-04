#!/usr/bin/env bash
set -euo pipefail

# This is the same deterministic check used by collect-result.sh. Teaching
# platforms can invoke it as the first-pass grader before a human reads logs
# and the service-boundary explanation in the report.
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
bash "$ROOT/scripts/up.sh"
bash "$ROOT/scripts/verify.sh"
bash "$ROOT/scripts/fault-test.sh"
echo "AUTOGRADER: PASS"

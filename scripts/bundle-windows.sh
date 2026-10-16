#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ANTIMONY_DIR="${1:-build/antimony}"
MSYS_BIN="${2:-/ucrt64/bin}"

python3 "$SCRIPT_DIR/deploy-windows.py" "$ANTIMONY_DIR" "$MSYS_BIN"

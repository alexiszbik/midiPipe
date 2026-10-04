#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="${ROOT}/build"

if [[ -z "${JOBS:-}" ]]; then
  if command -v sysctl >/dev/null 2>&1; then
    JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
  else
    JOBS="$(nproc 2>/dev/null || echo 4)"
  fi
fi

cmake -S "${ROOT}" -B "${BUILD}"
cmake --build "${BUILD}" -j "${JOBS}"

exec "${BUILD}/midiPipe" "$@"

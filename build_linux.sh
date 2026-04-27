#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${repo_root}/build"

cmake -S "${repo_root}" -B "${build_dir}" \
  -DOPENMS_THERMO_BRIDGE_ENABLE_VENDOR_DOWNLOAD=ON \
  -DOPENMS_THERMO_BRIDGE_DOWNLOAD_TEST_DATA=ON
cmake --build "${build_dir}" --parallel
ctest --test-dir "${build_dir}" --output-on-failure

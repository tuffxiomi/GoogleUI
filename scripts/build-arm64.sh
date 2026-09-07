#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PRELOADER_SO="${PRELOADER_SO:-}"
BUILD_DIR="${ROOT}/build-arm64"

if [[ -z "${PRELOADER_SO}" || ! -f "${PRELOADER_SO}" ]]; then
    echo "PRELOADER_SO must point to the exact ARM64 libpreloader.so used by the target LeviLauncher." >&2
    exit 2
fi

EXPECTED_SHA256="8d628e4251498a867f9058c045068207616af170dde7b0962528fd4d54e47fed"
ACTUAL_SHA256="$(sha256sum "${PRELOADER_SO}" | awk '{print $1}')"
if [[ "${ACTUAL_SHA256}" != "${EXPECTED_SHA256}" ]]; then
    echo "Refusing unverified libpreloader.so: expected ${EXPECTED_SHA256}, got ${ACTUAL_SHA256}" >&2
    exit 3
fi

python3 "${ROOT}/scripts/generate-adblock-dex.py" \
  --dex "${ROOT}/resources/adblock_client.dex" \
  --header "${ROOT}/include/AdBlockClientDex.hpp" \
  --source "${ROOT}/src/AdBlockClientDex.cpp"

python3 "${ROOT}/scripts/generate-adblock-data.py" \
  --template "${ROOT}/resources/adblock/universal.js" \
  --hosts "${ROOT}/resources/adblock/hosts.txt" \
  --tokens "${ROOT}/resources/adblock/url_tokens.txt" \
  --source "${ROOT}/src/AdBlockData.cpp" \
  --runtime-js "${BUILD_DIR}/adblock_runtime.js"

if command -v node >/dev/null 2>&1; then
    node --check "${BUILD_DIR}/adblock_runtime.js"
fi

rm -rf "${BUILD_DIR}"
cmake -S "${ROOT}" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DPRELOADER_SO="${PRELOADER_SO}"
cmake --build "${BUILD_DIR}" --target levi_package --parallel

python3 "${ROOT}/scripts/verify-package.py" "${BUILD_DIR}/libGoogleUI_V1.0.0.levipack"
echo "Built: ${BUILD_DIR}/libGoogleUI_V1.0.0.levipack"

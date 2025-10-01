#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 3 ]]; then
  echo "Usage: $0 <build-dir> <output-dir> <codesign-identity> [entitlements]" >&2
  exit 1
fi

BUILD_DIR="$1"
OUTPUT_DIR="$2"
IDENTITY="$3"
ENTITLEMENTS="${4:-}"

APP_BUNDLE_SRC="${BUILD_DIR}/symbolcast-desktop.app"
SERVICE_BUNDLE_SRC="${BUILD_DIR}/symbolcast-service-macos.app"
APP_BUNDLE_DEST="${OUTPUT_DIR}/SymbolCast.app"
SERVICE_DIR_DEST="${APP_BUNDLE_DEST}/Contents/Library/Services"
SERVICE_BUNDLE_DEST="${SERVICE_DIR_DEST}/SymbolCast Service.app"

if [[ ! -d "${APP_BUNDLE_SRC}" ]]; then
  echo "Desktop bundle not found at ${APP_BUNDLE_SRC}. Build the project with CMake on macOS first." >&2
  exit 2
fi

if [[ ! -d "${SERVICE_BUNDLE_SRC}" ]]; then
  echo "Service bundle not found at ${SERVICE_BUNDLE_SRC}. Build the project with CMake on macOS first." >&2
  exit 3
fi

mkdir -p "${OUTPUT_DIR}"
rm -rf "${APP_BUNDLE_DEST}"
cp -R "${APP_BUNDLE_SRC}" "${APP_BUNDLE_DEST}"

mkdir -p "${SERVICE_DIR_DEST}"
rm -rf "${SERVICE_BUNDLE_DEST}"
cp -R "${SERVICE_BUNDLE_SRC}" "${SERVICE_BUNDLE_DEST}"

SIGN_ARGS=(--force --options runtime --sign "${IDENTITY}")
if [[ -n "${ENTITLEMENTS}" ]]; then
  SIGN_ARGS+=(--entitlements "${ENTITLEMENTS}")
fi

codesign "${SIGN_ARGS[@]}" "${SERVICE_BUNDLE_DEST}"
codesign "${SIGN_ARGS[@]}" "${APP_BUNDLE_DEST}"

echo "Packaged ${APP_BUNDLE_DEST} with bundled macOS Service." >&2

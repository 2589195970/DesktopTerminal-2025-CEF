#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
DEPS_DIR="$ROOT_DIR/resources/dependencies"
mkdir -p "$DEPS_DIR"

declare -A URLS=(
    [VC_redist.x86.exe]="https://aka.ms/vs/17/release/vc_redist.x86.exe"
    [VC_redist.x64.exe]="https://aka.ms/vs/17/release/vc_redist.x64.exe"
)

for file in "${!URLS[@]}"; do
    url="${URLS[$file]}"
    echo "Downloading $file ..."
    curl -L "$url" -o "$DEPS_DIR/$file"
    shasum -a 256 "$DEPS_DIR/$file"
    echo
done

echo "所有离线运行时已下载到 $DEPS_DIR"

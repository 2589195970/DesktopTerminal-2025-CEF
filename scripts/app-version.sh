#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
version=$(sed -nE 's/^#define DESKTOP_TERMINAL_VERSION "([0-9]+\.[0-9]+\.[0-9]+)"[[:space:]]*$/\1/p' "$SCRIPT_DIR/../src/app_version.h")
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "src/app_version.h 必须定义一个形如 主版本.次版本.修订号 的版本号" >&2
    exit 1
fi
printf '%s\n' "$version"

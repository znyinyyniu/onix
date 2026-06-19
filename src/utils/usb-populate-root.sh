#!/bin/bash
# 填充 U 盘 Minix 根分区，用法: usb-populate-root.sh <挂载点>
set -e
ROOT="$1"
SRC="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$(cd "$SRC/../build" && pwd)"

mkdir -p "$ROOT/bin" "$ROOT/dev" "$ROOT/etc" "$ROOT/mnt" "$ROOT/data"
touch "$ROOT/dev/.keep"
cp -r "$SRC/utils/network.conf" "$ROOT/etc/"
cp -r "$SRC/utils/resolv.conf" "$ROOT/etc/"
cp "$BUILD/mono.wav" "$ROOT/data/" 2>/dev/null || true
cp "$BUILD/stereo.wav" "$ROOT/data/" 2>/dev/null || true
for app in "$BUILD"/builtin/*.out; do
    [ -f "$app" ] && cp "$app" "$ROOT/bin/"
done
echo "hello onix!!!" > "$ROOT/hello.txt"

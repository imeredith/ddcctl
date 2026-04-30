#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
  printf 'usage: %s <version>\n' "$0" >&2
  printf 'example: %s 0.1.0\n' "$0" >&2
  exit 64
fi

version="${1#v}"
name="ddcctl-v${version}-macos-arm64"
dist="dist"
stage="${dist}/${name}"
archive="${dist}/${name}.tar.gz"

swift build -c release

rm -rf "$stage" "$archive"
mkdir -p "$stage"
cp .build/release/ddcctl "$stage/ddcctl"
chmod 755 "$stage/ddcctl"

tar -C "$dist" -czf "$archive" "$name"
shasum -a 256 "$archive"

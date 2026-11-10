#!/bin/sh
# Copies the logs a failed job leaves into a directory, for CircleCI to keep: each build's test logs,
# JUnit report, and vcpkg install log, under their paths in the checkout, and vcpkg's build logs,
# under vcpkg/.
#
#   ci/gather-logs.sh logs
set -eu

destination=$1
for path in build/*/Testing/Temporary build/*/junit.xml build/*/vcpkg-manifest-install.log; do
    if [ -e "$path" ]; then
        mkdir -p "$destination/$(dirname "$path")"
        cp -R "$path" "$destination/$(dirname "$path")/"
    fi
done
for log in "${VCPKG_ROOT:-/nonexistent}"/buildtrees/*/*.log; do
    if [ -e "$log" ]; then
        port=$(basename "$(dirname "$log")")
        mkdir -p "$destination/vcpkg/$port"
        cp "$log" "$destination/vcpkg/$port/"
    fi
done

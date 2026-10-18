#!/bin/sh
# Prints the tag of the CI image: git's hash of ci/linux/ as committed at HEAD. Any change in that
# directory changes the tag, and nothing outside it does. A tag with uncommitted changes under it
# would name an image that was never built from them, so they stop the script.
set -eu

root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)

if [ -n "$(git -C "$root" status --porcelain --untracked-files=all -- ci/linux)" ]; then
    echo "image-tag: ci/linux/ has uncommitted changes; commit them first" >&2
    exit 1
fi

git -C "$root" rev-parse HEAD:ci/linux

#!/bin/bash
# Given a fresh struct_dump.gdb output for one PHP version on one arch,
# patch the corresponding checked-in mirror header in place.
#
# Usage: tools/render_struct_update.sh <nn> <arch> <raw_dump_file> [repo_root]
#
# Exit status: 0 = header already matched or was patched cleanly.
#              1 = patch_struct_header.awk flagged something for human
#                  review (see stderr); the header is left untouched.

set -euo pipefail

this_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)
nn=$1
arch=$2
raw_dump=$3
repo_root=${4:-$this_dir/..}

header="$repo_root/structs/$arch/php_structs_$nn.h"
if [ ! -f "$header" ]; then
    echo "render_struct_update: no such header: $header" >&2
    exit 2
fi

offsets=$(mktemp)
trap 'rm -f "$offsets"' EXIT
gawk -f "$this_dir/parse_struct_dump.awk" "$raw_dump" > "$offsets"

patched=$(mktemp)
if gawk -f "$this_dir/patch_struct_header.awk" -v dumpfile="$offsets" "$header" > "$patched" 2>"$patched.log"; then
    rv=0
else
    rv=1
fi
cat "$patched.log" >&2

if ! diff -q "$patched" "$header" >/dev/null 2>&1; then
    cp "$patched" "$header"
    echo "render_struct_update: updated $header" >&2
fi
rm -f "$patched" "$patched.log"

exit "$rv"

#!/bin/bash
# Print struct_dump.sh's all_phpvs array, one tag per line. Single source
# of truth for "every version phpspy currently ships struct mirrors for" --
# both struct_dump.sh itself and .github/workflows/dump_structs.yml's matrix
# read from here rather than keeping their own separate copies of the list.
#
# Usage: tools/list_pinned_tags.sh [struct_dump.sh path]

set -euo pipefail

this_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)
dump_sh="${1:-$this_dir/../struct_dump.sh}"

awk '
    /^all_phpvs=\(/ { in_arr = 1; next }
    in_arr && /^\)/ { in_arr = 0; next }
    in_arr { gsub(/^[ \t]+|[ \t]+$/, ""); if (length($0)) print }
' "$dump_sh"

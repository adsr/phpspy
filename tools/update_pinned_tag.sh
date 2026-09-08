#!/bin/bash
# Update struct_dump.sh's pinned tag for whichever series <new_tag> belongs
# to (e.g. "php-8.1.34" replaces whatever "php-8.1.NNN" line is currently
# pinned). PHP 8.6 is a special case: it's pinned to the literal `master`
# until a real release tag exists, so a `php-8.6.*` tag replaces that
# literal line instead of a numbered one.
#
# Usage: tools/update_pinned_tag.sh <new_tag> [struct_dump.sh path]

set -euo pipefail

new_tag=$1
this_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)
dump_sh="${2:-$this_dir/../struct_dump.sh}"

if [[ "$new_tag" =~ ^php-8\.6\. ]]; then
    if ! grep -qE '^ *master *$' "$dump_sh"; then
        echo "update_pinned_tag: no literal 'master' line found for 8.6" >&2
        exit 1
    fi
    sed -i -E "s/^( *)master( *)$/\1${new_tag}\2/" "$dump_sh"
else
    series=$(echo "$new_tag" | sed -E 's/\.[0-9]+$//')
    series_escaped=$(printf '%s' "$series" | sed -E 's/[.[\*^$]/\\&/g')
    if ! grep -qE "^ *${series_escaped}\.[0-9]+ *$" "$dump_sh"; then
        echo "update_pinned_tag: no existing ${series}.NNN line found to replace" >&2
        exit 1
    fi
    sed -i -E "s/^( *)${series_escaped}\.[0-9]+( *)$/\1${new_tag}\2/" "$dump_sh"
fi

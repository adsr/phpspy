#!/bin/bash
# Compare the PHP release tags pinned in struct_dump.sh against what's
# actually tagged upstream, and report any series that has moved.
#
# Prints one "NN TAG" line per series that needs a fresh dump (NN = the
# phpspy-internal two-digit version, TAG = the newer php-src tag). Prints
# nothing if everything is current. On stderr, separately notes any PHP
# major.minor series that exists upstream but isn't tracked by phpspy at
# all (e.g. a future 8.7) -- that needs a human, not this script, since
# supporting a new series is more than a struct-offset refresh.
#
# Usage: tools/find_php_updates.sh [path-to-struct_dump.sh]

set -euxo pipefail

this_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)
dump_sh="${1:-$this_dir/../struct_dump.sh}"

declare -A series=(
    [70]=7.0 [71]=7.1 [72]=7.2 [73]=7.3 [74]=7.4
    [80]=8.0 [81]=8.1 [82]=8.2 [83]=8.3 [84]=8.4 [85]=8.5
)

mapfile -t all_tags < <(
    # -c ...extraheader= clears any Authorization header a caller's git
    # config may have set for github.com URLs in general (e.g. a CI
    # checkout step's persisted credentials, scoped to a *different* repo)
    # -- inherited into a request for an unrelated public repo like this
    # one, such a header gets rejected outright rather than ignored.
    git -c http.https://github.com/.extraheader= \
        ls-remote --tags https://github.com/php/php-src.git 'refs/tags/php-*' \
        | sed 's#.*refs/tags/##' \
        | grep -E '^php-[0-9]+\.[0-9]+\.[0-9]+$'
)

for nn in "${!series[@]}"; do
    prefix="php-${series[$nn]}."
    latest=$(printf '%s\n' "${all_tags[@]}" | grep -F "$prefix" | sort -V | tail -1)
    [ -z "$latest" ] && continue
    pinned=$(grep -oE "php-${series[$nn]//./\\.}\.[0-9]+" "$dump_sh" | head -1)
    if [ "$latest" != "$pinned" ]; then
        echo "$nn $latest"
    fi
done

# 8.6 is pinned to `master` (no release tag yet) -- flag it the moment one exists.
php86tag=$(printf '%s\n' "${all_tags[@]}" | grep -E '^php-8\.6\.' | sort -V | tail -1)
if [ -n "$php86tag" ]; then
    echo "86 $php86tag"
fi

# Informational only: a series beyond what phpspy tracks at all.
newest_series=$(printf '%s\n' "${all_tags[@]}" | grep -oE '^php-[0-9]+\.[0-9]+' | sed 's/^php-//' | sort -V -u | tail -1)
known_max="8.6"
if [ "$(printf '%s\n%s\n' "$known_max" "$newest_series" | sort -V | tail -1)" != "$known_max" ]; then
    echo "note: upstream has a newer series ($newest_series) than phpspy tracks (up to $known_max) -- this needs a human to add support, not an automated struct refresh" >&2
fi

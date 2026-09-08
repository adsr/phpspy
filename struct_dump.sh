#!/bin/bash
this_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)
phpsrc_dir=$1
only_phpv=$2

if [ -z "$phpsrc_dir" ]; then
    echo "Required: php-src directory"
    exit 1
fi

# All PHP versions phpspy ships struct mirrors for. 8.6 has no release tag yet
# (still dev), so it stays pinned to `master`, same as when the 8.6 structs
# were first cut.
all_phpvs=(
    php-7.0.33
    php-7.1.33
    php-7.2.34
    php-7.3.33
    php-7.4.33
    php-8.0.30
    php-8.1.28
    php-8.2.18
    php-8.3.6
    php-8.4.25
    php-8.5.10
    master
)

if [ -n "$only_phpv" ]; then
    phpvs=("$only_phpv")
else
    phpvs=("${all_phpvs[@]}")
fi

pushd "$phpsrc_dir" || exit 1
git fetch --tags
for phpv in "${phpvs[@]}"
do
    git reset --hard HEAD \
        && git clean -fdx \
        && git checkout "$phpv" \
        && git clean -fdx \
        && ./buildconf --force \
        && ./configure \
        && make -j "$(grep -c '^proc' /proc/cpuinfo)" \
        && gdb -batch -ex "printf \"$phpv\n\n\"" -x "$this_dir/struct_dump.gdb" --args ./sapi/cli/php >"$this_dir/struct_dump.$phpv.out"
done
popd || exit 1

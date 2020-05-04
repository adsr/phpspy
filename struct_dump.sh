#!/bin/bash
this_dir=$(cd $(dirname "${BASH_SOURCE[0]}") >/dev/null && pwd)
phpsrc_dir=$1

if [ -z "$phpsrc_dir" ]; then
    echo "Required: php-src directory"
    exit 1
fi

pushd $phpsrc_dir
git fetch --tags
for phpv in php-7.0.33 php-7.1.33 php-7.2.30 php-7.3.17 php-7.4.5; do
    git reset --hard HEAD \
        && git clean -fdx \
        && git checkout $phpv \
        && git clean -fdx \
        && ./buildconf --force \
        && ./configure \
        && make -j$(grep -c '^proc' /proc/cpuinfo) \
        && gdb -batch -ex "printf \"$phpv\n\n\"" -x "$this_dir/struct_dump.gdb" --args ./sapi/cli/php >"$this_dir/struct_dump.$phpv.out"
done
popd

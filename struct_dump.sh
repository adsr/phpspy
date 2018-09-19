#!/bin/bash
this_dir=$(cd $(dirname "${BASH_SOURCE[0]}") >/dev/null && pwd)
phpsrc_dir=~/php-src

pushd $phpsrc_dir
git fetch --tags
for phpv in php-7.0.29 php-7.1.21 php-7.2.9 php-7.3.0beta3; do
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

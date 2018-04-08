#!/bin/bash

pushd ~/php-src
for phpv in php-7.2.4 php-7.1.16 php-7.0.29; do
    git reset --hard HEAD && \
        git clean -fdx && \
        git checkout $phpv && \
        git clean -fdx && \
        ./buildconf --force && \
        ./configure && \
        make -j$(grep -c '^proc' /proc/cpuinfo) && \
        gdb -batch -ex "printf \"$phpv\n\n\"" -x ~/phpspy/struct_dump.gdb --args ./sapi/cli/php >>~/phpspy/struct_dump.out
done
popd

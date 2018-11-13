#!/bin/bash

if { objdump -tT $(command -v $PHP) | grep -q alloc_globals; }; then
    phpspy_opts=(--memory-usage -- $PHP -r 'sleep(1);')
    declare -A expected
    expected[mem]='^# mem \d+ \d+$'
else
    skip='alloc_globals symbol not found'
fi
source $TEST_SH

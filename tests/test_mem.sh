#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck source=/dev/null
source "$TEST_SH"

if { $PHPSPY -v | grep -q USE_ZEND=y; }; then
    skip='alloc_globals not available in USE_ZEND=y build'
elif { objdump -tT "$(command -v "${PHP[0]}")" | grep -q alloc_globals; }; then
    phpspy_opts=(--memory-usage -- "${PHP[@]}" -r 'sleep(1);')
    declare -A expected
    expected[mem]='^# mem \d+ \d+$'
else
    skip='alloc_globals symbol not found'
fi
test_invoke

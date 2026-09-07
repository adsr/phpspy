#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck source=/dev/null
source "$TEST_SH"

if { $PHPSPY -v | grep -q USE_ZEND=y; }; then
    test_skip='alloc_globals not available in USE_ZEND=y build'
elif { objdump -tT "$(command -v "${PHP[0]}")" | grep -q alloc_globals; }; then
    test_phpspy_opts=(--memory-usage -- "${PHP[@]}" -r 'sleep(1);')
    declare -A test_expected
    test_expected[mem]='^# mem \d+ \d+$'
else
    test_skip='alloc_globals symbol not found'
fi
test_invoke

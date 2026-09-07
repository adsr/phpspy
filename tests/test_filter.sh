#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck disable=SC2016 # ignore $var in single quote strings
# shellcheck source=/dev/null
source "$TEST_SH"

test_phpspy_opts=(--limit=0 --filter '(nano|poop)sleep' -- "${PHP[@]}" -r '$n=10000; while(--$n) {usleep(1); time_nanosleep(0, 1000);}')
declare -A test_expected
declare -A test_not_expected
test_expected[include_nanosleep]='nanosleep'
test_not_expected[exclude_usleep]='usleep'
test_invoke

test_phpspy_opts=(--limit=0 --filter-negate 'usle+[pP]' -- "${PHP[@]}" -r '$n=10000; while(--$n) {usleep(1); time_nanosleep(0, 1000);}')
declare -A test_expected
declare -A test_not_expected
test_expected[negate_include_nanosleep]='nanosleep'
test_not_expected[negate_exclude_usleep]='usleep'
test_invoke

test_phpspy_opts=(--limit=1 --filter 'usleep' -- "${PHP[@]}" -r 'time_nanosleep(1, 0); usleep(1000000);')
declare -A test_expected
declare -A test_not_expected
test_expected[limit_include_usleep]='usleep'
test_not_expected[limit_exclude_nanosleep]='time_nanosleep'
test_invoke

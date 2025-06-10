#!/bin/bash

phpspy_opts=(--limit=0 --filter '(nano|poop)sleep' -- $PHP -r '$n=10000; while(--$n) {usleep(1); time_nanosleep(0, 1000);}')
declare -A expected
declare -A not_expected
expected[include_nanosleep]='nanosleep'
not_expected[exclude_usleep]='usleep'
source $TEST_SH

phpspy_opts=(--limit=0 --filter-negate 'usle+[pP]' -- $PHP -r '$n=10000; while(--$n) {usleep(1); time_nanosleep(0, 1000);}')
declare -A expected
declare -A not_expected
expected[negate_include_nanosleep]='nanosleep'
not_expected[negate_exclude_usleep]='usleep'
source $TEST_SH

phpspy_opts=(--limit=1 --filter 'usleep' -- $PHP -r 'time_nanosleep(1, 0); usleep(1000000);')
declare -A expected
declare -A not_expected
expected[limit_include_usleep]='usleep'
not_expected[limit_exclude_nanosleep]='time_nanosleep'
source $TEST_SH

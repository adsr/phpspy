#!/bin/bash

phpspy_opts=(--limit=0 --filter '(nano|poop)sleep' -- $PHP -r '$n=10000; while(--$n) {usleep(1); time_nanosleep(0, 1000);}')
declare -A expected
declare -A not_expected
expected[include_nanosleep]='nanosleep'
not_expected[exclude_usleep]='usleep'
source $TEST_SH

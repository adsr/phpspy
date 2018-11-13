#!/bin/bash

phpspy_opts=(--memory-usage -- $PHP -r 'sleep(1);')
declare -A expected
expected[mem]='^# mem \d+ \d+$'
source $TEST_SH

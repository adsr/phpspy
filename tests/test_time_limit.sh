#!/bin/bash

$PHP -r 'sleep(4);' &
php_pid=$!
phpspy_opts=(--limit=0 --time-limit-ms=1000 --pid=$php_pid)
declare -A expected
expected[include_sleep]='sleep'
use_sudo=1
use_timeout_s=2
source $TEST_SH
wait $php_pid

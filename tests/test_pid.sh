#!/bin/bash

$PHP -r 'sleep(1);' &
php_pid=$!
phpspy_opts=(--pid $php_pid)
declare -A expected
expected[frame_0        ]='^0 sleep <internal>:-1$'
expected[frame_1        ]='^1 <main> <internal>:-1$'
use_sudo=1
source $TEST_SH
wait $php_pid

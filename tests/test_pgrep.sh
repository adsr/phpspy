#!/bin/bash

$PHP -r 'sleep(1);' &
php_pid=$!
pid_file=$(mktemp)
echo "$php_pid" >$pid_file
phpspy_opts=(--pgrep "--pidfile $pid_file" --threads 2 --event-handler-opts m)
declare -A expected
expected[frame_0        ]='^0 sleep <internal>:-1$'
expected[frame_1        ]='^1 <main> <internal>:-1$'
use_timeout_s=2
use_sudo=1
non_zero_ok=1
source $TEST_SH
wait $php_pid
rm -f $pid_file

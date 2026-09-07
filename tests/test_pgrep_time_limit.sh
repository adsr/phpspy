#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck source=/dev/null
source "$TEST_SH"

"${PHP[@]}" -r 'sleep(4);' &
php_pid=$!
pid_file=$(mktemp)
echo "$php_pid" >"$pid_file"
test_phpspy_opts=(--pgrep "--pidfile $pid_file" --threads 2 --time-limit-ms=1000)
declare -A test_expected
test_expected[frame_0        ]='^0 sleep <internal>:-1$'
test_expected[frame_1        ]='^1 <main> <internal>:-1$'
test_use_timeout_s=2
test_need_ptrace=1
test_invoke
wait $php_pid
rm -f "$pid_file"

test_phpspy_opts=(--pgrep "--full hope_this_does_not_exist_lol" --threads 2 --time-limit-ms=1000)
declare -A test_expected
test_expected[nothing]='^$'
test_use_timeout_s=2
test_need_ptrace=1
test_invoke
wait $php_pid
rm -f "$pid_file"

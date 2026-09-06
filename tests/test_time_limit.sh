#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck source=/dev/null
source "$TEST_SH"

"${PHP[@]}" -r 'sleep(4);' &
php_pid=$!
phpspy_opts=(--limit=0 --time-limit-ms=1000 --pid="$php_pid")
declare -A expected
expected[include_sleep]='sleep'
need_ptrace=1
use_timeout_s=2
test_invoke
wait $php_pid

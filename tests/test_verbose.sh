#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck source=/dev/null
source "$TEST_SH"

phpspy_opts=(--verbose-fields=pt -- "${PHP[@]}" -r 'usleep(1000000);')
declare -A expected
expected[frame_0        ]='^0 usleep <internal>:-1$'
expected[frame_1        ]='^1 <main> <internal>:-1$'
expected[verbose_ts     ]='^# trace_ts = \d+\.\d+'
expected[verbose_pid    ]='^# pid = \d+$'
test_invoke

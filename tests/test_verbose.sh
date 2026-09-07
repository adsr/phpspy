#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck source=/dev/null
source "$TEST_SH"

test_phpspy_opts=(--verbose-fields=pt -- "${PHP[@]}" -r 'usleep(1000000);')
declare -A test_expected
test_expected[frame_0        ]='^0 usleep <internal>:-1$'
test_expected[frame_1        ]='^1 <main> <internal>:-1$'
test_expected[verbose_ts     ]='^# trace_ts = \d+\.\d+'
test_expected[verbose_pid    ]='^# pid = \d+$'
test_invoke

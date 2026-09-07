#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck source=/dev/null
source "$TEST_SH"

test_phpspy_opts=(--continue-on-error --buffer-size=24 -- "${PHP[@]}" -r 'usleep(1000000);')
declare -A test_expected
declare -A test_not_expected
test_expected[frame_0        ]='^0 usleep <internal>:-1$'
test_not_expected[no_frame_1 ]='^1 <main> <internal>:-1$'
test_invoke

test_phpspy_opts=(--buffer-size=24 -- "${PHP[@]}" -r 'usleep(1000000);')
declare -A test_not_expected
test_not_expected[anything]='^.+$'
test_invoke

#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck source=/dev/null
source "$TEST_SH"

phpspy_opts=(--continue-on-error --buffer-size=24 -- "${PHP[@]}" -r 'usleep(1000000);')
declare -A expected
declare -A not_expected
expected[frame_0        ]='^0 usleep <internal>:-1$'
not_expected[no_frame_1 ]='^1 <main> <internal>:-1$'
test_invoke

phpspy_opts=(--buffer-size=24 -- "${PHP[@]}" -r 'usleep(1000000);')
declare -A not_expected
not_expected[anything]='^.+$'
test_invoke

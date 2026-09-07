#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck source=/dev/null
source "$TEST_SH"

test_phpspy_opts=(--request-info=qcup -- "${PHP[@]}" -r 'usleep(1000000);')
declare -A test_expected
test_expected[frame_0        ]='^0 usleep <internal>:-1$'
test_expected[frame_1        ]='^1 <main> <internal>:-1$'
test_expected[req_uri        ]='^# uri = -$'
test_expected[req_path       ]='^# path = (Standard input code|-)$'
test_expected[req_qstring    ]='^# qstring = -$'
test_expected[req_cookie     ]='^# cookie = -$'
test_expected[req_ts         ]='^# ts = \d+\.\d+$'
test_invoke

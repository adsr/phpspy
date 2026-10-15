#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck disable=SC2016 # ignore $var in single quote strings
# shellcheck source=/dev/null
source "$TEST_SH"

# Nothing can fit in --buffer-size=1, so everything is truncated
test_phpspy_opts=(--buffer-size=1 --limit=10 -- "${PHP[@]}" -r 'usleep(1000000);')
declare -A test_expected
declare -A test_not_expected
test_expected[b1_trunc]='^# truncated = 1$'
test_expected[b1_sentinel_bang]='^# trace_id = .*!$'
test_not_expected[b1_no_sentinel_tilde]='^# trace_id = .*~$'
test_not_expected[b1_no_frame]='^[0-9] '
test_invoke

# echo -n $'0 usleep <internal>:-1\n# trace_id = 0.0~\n\n' | wc -c # 42
# Beyond 10 traces, we'd need 43 bytes for 2-digit trace_id
test_phpspy_opts=(--buffer-size=42 --limit=10 -- "${PHP[@]}" -r 'usleep(1000000);')
declare -A test_expected
declare -A test_not_expected
test_expected[b42_chunk1]='^# trace_id = 0\.0~$'
test_expected[b42_chunk2]='^# trace_id = 0\.1!$'
test_expected[b42_frame_0]='^0 usleep <internal>:-1$'
test_expected[b42_frame_1]='^1 <main> <internal>:-1$'
test_not_expected[b42_no_trunc]='^# truncated = 1$'
test_invoke

# 4k is ample space for non-chunked non-truncated
test_phpspy_opts=(--buffer-size=4096 --limit=10 -- "${PHP[@]}" -r 'usleep(1000000);')
declare -A test_expected
declare -A test_not_expected
test_expected[b4k_frame_0]='^0 usleep <internal>:-1$'
test_expected[b4k_frame_1]='^1 <main> <internal>:-1$'
test_not_expected[b4k_no_trace]='^# trace_id ='
test_not_expected[b4k_no_trunc]='^# truncated = 1$'
test_invoke

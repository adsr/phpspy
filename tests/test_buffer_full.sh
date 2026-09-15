#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck disable=SC2016 # ignore $var in single quote strings
# shellcheck source=/dev/null
source "$TEST_SH"

php_src='function f($n){ if($n) f($n-1); else usleep(2000000); } f(20);'
# --limit=1 would race with the child's startup, whose stack is only `<main>`;
# sample for a while instead and assert over the whole capture
sample=(--limit=0 --time-limit-ms=1500)

# a deep trace no longer fits in one 128-byte write, but nothing is dropped
test_phpspy_opts=("${sample[@]}" --buffer-size=128 -- "${PHP[@]}" -r "$php_src")
declare -A test_expected test_not_expected
test_expected[frame_0      ]='^0 usleep <internal>:-1$'
test_expected[frame_22     ]='^22 <main>'
test_expected[marker       ]='^# trace_id = \d+\.\d+/\d+$'
test_expected[continuation ]='^# trace_id = \d+\.[1-9]\d*/\d+$'
test_not_expected[no_trunc ]='^# truncated'
test_invoke

# filter still applies to the whole assembled trace, before chunking
test_phpspy_opts=("${sample[@]}" --buffer-size=128 --filter '<main>' -- "${PHP[@]}" -r "$php_src")
declare -A test_expected
test_expected[filtered_frame_0]='^0 usleep <internal>:-1$'
test_expected[filtered_marker ]='^# trace_id = \d+\.\d+/\d+$'
test_invoke

test_phpspy_opts=("${sample[@]}" --buffer-size=128 --filter 'nomatch_xyz' -- "${PHP[@]}" -r "$php_src")
declare -A test_not_expected
test_not_expected[filtered_out_all]='^.+$'
test_use_timeout_s=10
test_invoke

#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck disable=SC2016 # ignore $var in single quote strings
# shellcheck source=/dev/null
source "$TEST_SH"

php_src='function f($n){ if($n) f($n-1); else usleep(2000000); } f(20);'

# -n alone: keep 2 frames from the leaf; no elision, no root
test_phpspy_opts=(--max-depth=2 -- "${PHP[@]}" -r "$php_src")
declare -A test_expected test_not_expected
test_expected[n_frame_0]='^0 usleep <internal>:-1$'
test_expected[n_frame_1]='^1 f Command line code:1$'
test_not_expected[n_no_marker]='^\d+ <elided'
test_not_expected[n_no_root]='^22 <main>'
test_invoke

# -N alone: keep 2 frames from the root; marker at depth 0, leaf absent
test_phpspy_opts=(--max-depth-outer=2 -- "${PHP[@]}" -r "$php_src")
declare -A test_expected test_not_expected
test_expected[N_marker_at_0]='^0 <elided:\d+> <elided>:-1$'
test_expected[N_frame_21]='^21 f Command line code:1$'
test_expected[N_frame_22]='^22 <main>'
test_not_expected[N_no_leaf]='^0 usleep <internal>:-1$'
test_invoke

# -n 2 -N 2: elides the middle (2+2=4 < 23 frames)
test_phpspy_opts=(--max-depth=2 --max-depth-outer=2 -- "${PHP[@]}" -r "$php_src")
declare -A test_expected test_not_expected
test_expected[mid_frame_0]='^0 usleep <internal>:-1$'
test_expected[mid_frame_1]='^1 f Command line code:1$'
test_expected[mid_marker]='^2 <elided:\d+> <elided>:-1$'
test_expected[mid_frame_21]='^21 f Command line code:1$'
test_expected[mid_frame_22]='^22 <main>'
test_not_expected[mid_no_frame_2]='^2 f Command line code:1$'
test_invoke

# -n 11 -N 12: exactly fits 23 frames (11+12=23), no elision
test_phpspy_opts=(--max-depth=11 --max-depth-outer=12 -- "${PHP[@]}" -r "$php_src")
declare -A test_expected test_not_expected
test_expected[exact_frame_0]='^0 usleep <internal>:-1$'
test_expected[exact_frame_10]='^10 f Command line code:1$'
test_expected[exact_frame_11]='^11 f Command line code:1$'
test_expected[exact_frame_22]='^22 <main>'
test_not_expected[exact_no_marker]='^\d+ <elided'
test_invoke

# -n 15 -N 15: overlapping (15+15=30 > 23), all frames, no elision
test_phpspy_opts=(--max-depth=15 --max-depth-outer=15 -- "${PHP[@]}" -r "$php_src")
declare -A test_expected test_not_expected
test_expected[over_frame_0]='^0 usleep <internal>:-1$'
test_expected[over_frame_22]='^22 <main>'
test_not_expected[over_no_marker]='^\d+ <elided'
test_invoke

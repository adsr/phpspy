#!/bin/bash

php_src='function f($n){ if($n) f($n-1); else usleep(2000000); } f(20);'

# -n alone keeps the innermost frames, as before, with no elision marker.
phpspy_opts=(--max-depth=2 -- $PHP -r "$php_src")
declare -A expected
declare -A not_expected
expected[frame_0        ]='^0 usleep <internal>:-1$'
not_expected[no_marker  ]='^\d+ <elided'
not_expected[no_outer   ]='^22 <main> <internal>:-1$'
source $TEST_SH

# -N keeps the outermost frames. The marker must take depth 0, because
# stackcollapse-phpspy.pl treats depth 0 as the start of a trace.
phpspy_opts=(--max-depth-outer=2 -- $PHP -r "$php_src")
declare -A expected
declare -A not_expected
expected[marker_at_0    ]='^0 <elided:\d+> <elided>:-1$'
expected[outermost      ]='^22 <main> <internal>:-1$'
not_expected[no_innermost]='^0 usleep <internal>:-1$'
source $TEST_SH

# Both ends, middle elided.
phpspy_opts=(--max-depth=2 --max-depth-outer=2 -- $PHP -r "$php_src")
declare -A expected
expected[frame_0        ]='^0 usleep <internal>:-1$'
expected[frame_1        ]='^1 f Command line code:1$'
expected[marker         ]='^2 <elided:\d+> <elided>:-1$'
expected[outermost      ]='^22 <main> <internal>:-1$'
source $TEST_SH

# A stack shallower than the cap must not be elided at all.
phpspy_opts=(--max-depth-outer=10 -- $PHP -r 'usleep(2000000);')
declare -A expected
declare -A not_expected
expected[frame_0        ]='^0 usleep <internal>:-1$'
expected[frame_1        ]='^1 <main> <internal>:-1$'
not_expected[no_marker  ]='<elided'
source $TEST_SH

# Regression guard for the format trap: without a depth-0 line every trace in
# the file would collapse into a single stack. Assert each sample is counted.
collapse_out=$(mktemp)
on_exit() { rm -f $collapse_out; }
trap on_exit EXIT

REPO=$(dirname $(dirname $TEST_SH))
$PHPSPY --limit=0 --time-limit-ms=1500 --max-depth-outer=3 --warn-no-traces-s=0 \
    -O/dev/null -E/dev/null -- $PHP -r "$php_src" >$collapse_out 2>/dev/null

n_traces=$(grep -c '^0 ' $collapse_out)
n_collapsed=$($REPO/stackcollapse-phpspy.pl <$collapse_out | awk '{s+=$NF} END{print s+0}')
if [ "$n_traces" -gt 0 -a "$n_traces" -eq "$n_collapsed" ]; then
    echo -e "  \x1b[32mOK  \x1b[0m stackcollapse ($n_traces traces, $n_collapsed samples)"
else
    echo -e "  \x1b[31mERR \x1b[0m stackcollapse\nexpected=$n_traces samples\n\nactual=$n_collapsed"
    exit 1
fi

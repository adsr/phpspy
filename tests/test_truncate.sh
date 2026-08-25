#!/bin/bash

php_src='function f($n){ if($n) f($n-1); else usleep(2000000); } f(20);'

# A deep stack that does not fit in --buffer-size must still be emitted, with
# the innermost frames kept and a marker appended.
phpspy_opts=(--buffer-size=256 -- $PHP -r "$php_src")
declare -A expected
expected[frame_0        ]='^0 usleep <internal>:-1$'
expected[truncated      ]='^# truncated = 1$'
source $TEST_SH

# A buffer big enough for the whole stack must not truncate.
phpspy_opts=(--buffer-size=65536 -- $PHP -r "$php_src")
declare -A expected
declare -A not_expected
expected[frame_0        ]='^0 usleep <internal>:-1$'
expected[outermost      ]='^22 <main> <internal>:-1$'
not_expected[no_marker  ]='^# truncated = 1$'
source $TEST_SH

# Truncated traces must still terminate properly, or downstream consumers merge
# them. Assert one trace delimiter (blank line) per depth-0 frame, and that
# every sample survives a stackcollapse round-trip.
flame_out=$(mktemp)
on_exit() { rm -f $flame_out; }
trap on_exit EXIT

$PHPSPY --limit=0 --time-limit-ms=2000 --buffer-size=256 \
    -O/dev/null -E/dev/null -- $PHP -r "$php_src" >$flame_out 2>/dev/null

n_traces=$(grep -c '^0 ' $flame_out)
n_delims=$(grep -c '^$' $flame_out)
if [ "$n_traces" -gt 0 -a "$n_traces" -eq "$n_delims" ]; then
    echo -e "  \x1b[32mOK  \x1b[0m trace_delims ($n_traces traces, $n_delims delimiters)"
else
    echo -e "  \x1b[31mERR \x1b[0m trace_delims\ntraces=$n_traces delimiters=$n_delims"
    exit 1
fi

REPO=$(dirname $(dirname $TEST_SH))
n_collapsed=$($REPO/stackcollapse-phpspy.pl <$flame_out | awk '{s+=$NF} END{print s+0}')
if [ "$n_collapsed" -eq "$n_traces" ]; then
    echo -e "  \x1b[32mOK  \x1b[0m stackcollapse ($n_collapsed samples)"
else
    echo -e "  \x1b[31mERR \x1b[0m stackcollapse\nexpected=$n_traces samples\n\nactual=$n_collapsed"
    exit 1
fi

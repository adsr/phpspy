#!/bin/bash

REPO=$(dirname $(dirname $TEST_SH))

on_exit() { rm -f $flame_svg; }
fail()    { echo -e "  \x1b[31mERR \x1b[0m $@"; exit 1; }

flame_svg=$(mktemp)
trap on_exit EXIT

$PHPSPY -O/dev/null -E/dev/null --request-info=qcup 2>/dev/null \
    -- $PHP -r 'sleep(2);' \
    | $REPO/stackcollapse-phpspy.pl \
    | $REPO/vendor/flamegraph.pl \
    > $flame_svg
grep -Pq '\d+ samples' $flame_svg || fail "Failed to generate flame graph"

$PHPSPY -O/dev/null -E/dev/null --request-info=QCUP 2>/dev/null \
    -- $PHP -r 'sleep(2);' \
    | $REPO/stackcollapse-phpspy.pl \
    | $REPO/vendor/flamegraph.pl \
    > $flame_svg
grep -Pq '\d+ samples' $flame_svg || fail "Failed to generate flame graph"

echo -e "  \x1b[32mOK  \x1b[0m flamegraph"

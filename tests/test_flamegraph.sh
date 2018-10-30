#!/bin/bash

REPO=$(dirname $(dirname $TEST_SH))


/usr/bin/php7.3 -r 'sleep(5);' &
_pid=$!
awk 'NR==1{path=$NF} /libphp7/{path=$NF} END{print path}' /proc/$_pid/maps
cat /proc/$_pid/maps
sleep 10

on_exit() { rm -f $flame_svg; }
fail()    { echo -e "  \x1b[31mERR\x1b[0m $@"; exit 1; }

flame_svg=$(mktemp)
trap on_exit EXIT

$PHPSPY --child-stdout=/dev/null --child-stderr=/dev/null --request-info=qcup \
    -- $PHP -r 'sleep(2);' \
    | $REPO/stackcollapse-phpspy.pl \
    | $REPO/vendor/flamegraph.pl \
    > $flame_svg
grep -Pq '\d+ samples' $flame_svg || fail "Failed to generate flame graph"

$PHPSPY --child-stdout=/dev/null --child-stderr=/dev/null --request-info=QCUP \
    -- $PHP -r 'sleep(2);' \
    | $REPO/stackcollapse-phpspy.pl \
    | $REPO/vendor/flamegraph.pl \
    > $flame_svg
grep -Pq '\d+ samples' $flame_svg || fail "Failed to generate flame graph"

echo -e "  \x1b[32mOK \x1b[0m flamegraph"

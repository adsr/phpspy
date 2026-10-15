#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck disable=SC2016 # ignore $var in single quote strings
# shellcheck source=/dev/null
source "$TEST_SH"

repo=$(dirname "$(dirname "$TEST_SH")")

_exit() { test -z "${flame_tmp:-}" || rm -f "$flame_tmp"; }
trap _exit EXIT

_flame() {
    flame_tmp=$(mktemp)

    "$PHPSPY" -O/dev/null -E/dev/null --request-info=qcup 2>/dev/null \
        -- "${PHP[@]}" -r 'sleep(2);' \
        | "$repo/stackcollapse-phpspy.pl" \
        | "$repo/vendor/flamegraph.pl" \
        > "$flame_tmp"
    test_assert_re "flamegraph_qcup" '\d+ samples' "$(cat "$flame_tmp")"

    "$PHPSPY" -O/dev/null -E/dev/null --request-info=QCUP 2>/dev/null \
        -- "${PHP[@]}" -r 'sleep(2);' \
        | "$repo/stackcollapse-phpspy.pl" \
        | "$repo/vendor/flamegraph.pl" \
        > "$flame_tmp"
    test_assert_re "flamegraph_QCUP" '\d+ samples' "$(cat "$flame_tmp")"

    "$PHPSPY" -O/dev/null -E/dev/null --buffer-size=128 2>/dev/null \
        -- "${PHP[@]}" -r '$f = fn($f, $n) => $n ? $f($f, $n - 1) : sleep(2); $f($f, 10);' \
        | "$repo/stackcollapse-phpspy.pl" \
        | "$repo/vendor/flamegraph.pl" \
        > "$flame_tmp"
    test_assert_re "flamegraph_chunked" '\d+ samples' "$(cat "$flame_tmp")"
    # all, main, 11 closures, sleep = 14 frames
    test_assert "flamegraph_chunked_frames" 14 "$(grep -Pc '\d+ samples' <"$flame_tmp")"

    local fname
    printf -v fname 'f%.0s' {1..200}
    # everything after the truncated long fname should be dropped
    "$PHPSPY" -O/dev/null -E/dev/null --buffer-size=128 2>/dev/null \
        -- "${PHP[@]}" -r "function $fname() { sleep(2); } function g() { $fname(); } g();" \
        | "$repo/stackcollapse-phpspy.pl" \
        > "$flame_tmp"
    test_assert 'flamegraph_trunc_no_long_fname' 0 "$(grep -Fcwe "$fname" <"$flame_tmp")"
    test_assert 'flamegraph_trunc_no_g' 0 "$(grep -Fcwe 'g' <"$flame_tmp")"
    test_assert 'flamegraph_trunc_no_main' 0 "$(grep -Fcwe '<main>' <"$flame_tmp")"
    test_assert 'flamegraph_trunc_yes_sleep' 0 "$(grep -Fqwe 'sleep' <"$flame_tmp"; echo $?)"
}
test_fn=_flame
test_invoke

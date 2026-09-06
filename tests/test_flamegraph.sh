#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck source=/dev/null
source "$TEST_SH"

_exit() { rm -f "$flame_svg"; }

_run() {
    local repo
    flame_svg=$(mktemp)
    repo=$(dirname "$(dirname "$TEST_SH")")
    trap _exit EXIT

    "$PHPSPY" -O/dev/null -E/dev/null --request-info=qcup 2>/dev/null \
        -- "${PHP[@]}" -r 'sleep(2);' \
        | "$repo/stackcollapse-phpspy.pl" \
        | "$repo/vendor/flamegraph.pl" \
        > "$flame_svg"
    test_assert_re "flamegraph (qcup)" '\d+ samples' "$(cat "$flame_svg")"

    "$PHPSPY" -O/dev/null -E/dev/null --request-info=QCUP 2>/dev/null \
        -- "${PHP[@]}" -r 'sleep(2);' \
        | "$repo/stackcollapse-phpspy.pl" \
        | "$repo/vendor/flamegraph.pl" \
        > "$flame_svg"
    test_assert_re "flamegraph (QCUP)" '\d+ samples' "$(cat "$flame_svg")"
}
test_fn=_run
test_invoke

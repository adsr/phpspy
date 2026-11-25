#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck disable=SC2069 # ignore redir warning in _phpspy_err
# shellcheck source=/dev/null
source "$TEST_SH"

_phpspy_err() { "$PHPSPY" -O/dev/null -E/dev/null -o /dev/null "$@" 2>&1 >/dev/null; }

_bad_addr() {
    local err
    err=$(_phpspy_err --stats --stats-warn-no-trace-s=1 --time-limit-ms=1000 \
        --rate-hz=5 --addr-executor-globals=1 \
        -- "${PHP[@]}" -r 'usleep(2000000);')
    test_assert_re warn 'captured 0 traces in 1s' "$err"
}
test_fn=_bad_addr
test_invoke

_no_stats() {
    local err
    err=$(_phpspy_err --stats-warn-no-trace-s=1 --time-limit-ms=1000 --rate-hz=5 \
        --addr-executor-globals=1 -- "${PHP[@]}" -r 'usleep(2000000);')
    test_assert no_warn 0 "$(grep -c 'captured 0 traces in' <<<"$err")"
}
test_fn=_no_stats
test_invoke

_healthy() {
    local err
    err=$(_phpspy_err --stats --stats-warn-no-trace-s=1 --time-limit-ms=1000 \
        -- "${PHP[@]}" -r 'usleep(2000000);')
    test_assert no_warn_healthy 0 "$(grep -c 'captured 0 traces in' <<<"$err")"
}
test_fn=_healthy
test_invoke

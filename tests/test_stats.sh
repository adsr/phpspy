#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck disable=SC2069 # ignore redir warning in _phpspy_err
# shellcheck source=/dev/null
source "$TEST_SH"

_phpspy_err() { "$PHPSPY" -O/dev/null -E/dev/null -o /dev/null "$@" 2>&1 >/dev/null; }

_basic() {
    local err
    err=$(_phpspy_err --stats --limit=1 \
        -- "${PHP[@]}" -r 'usleep(500000);')
    test_assert_re startup \
        '^stats_log_target: pid=\d+ version=\S+ executor_globals=0x[0-9a-f]+$' \
        "$err"
    test_assert_re summary \
        '^stats_log_report: \d+ samples in \d+\.\d+s across 1 pid\(s\): [1-9]\d* written' \
        "$err"
    test_assert_re ms_per_sample '\(\d+\.\d+ ms/sample\)$' "$err"
}
test_fn=_basic
test_invoke

_quiet() {
    local err
    err=$(_phpspy_err --stats --quiet --limit=1 \
        -- "${PHP[@]}" -r 'usleep(500000);')
    test_assert quiet_silences "" "$err"
}
test_fn=_quiet
test_invoke

_err_trace() {
    local err
    err=$(_phpspy_err --stats --time-limit-ms=1000 --rate-hz=5 \
        --addr-executor-globals=1 -- "${PHP[@]}" -r 'usleep(2000000);')
    test_assert_re counted_errored '[1-9]\d* errored'    "$err"
    test_assert_re zero_written    ' 0 written, 0 empty' "$err"
}
test_fn=_err_trace
test_invoke

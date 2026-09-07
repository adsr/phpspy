#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck source=/dev/null
source "$TEST_SH"

# exit 0: phpspy propagates child exit status
_exit_0() {
    "$PHPSPY" --limit=0 --quiet -O/dev/null -E/dev/null -o /dev/null \
        -- "${PHP[@]}" -r 'exit(0);' >/dev/null 2>&1
    test_assert exit_0 0 $?
}
test_fn=_exit_0
test_need_ptrace=1
test_invoke

# exit 3: non-zero status preserved
_exit_3() {
    "$PHPSPY" --limit=0 --quiet -O/dev/null -E/dev/null -o /dev/null \
        -- "${PHP[@]}" -r 'exit(3);' >/dev/null 2>&1
    test_assert exit_3 3 $?
}
test_fn=_exit_3
test_need_ptrace=1
test_invoke

# signal: exit status is 128 + signal number (SIGTERM=15, expect 143)
_signalled() {
    "$PHPSPY" --limit=0 --quiet -O/dev/null -E/dev/null -o /dev/null \
        -- "${PHP[@]}" \
        -r 'posix_kill(posix_getpid(), SIGTERM); usleep(5000000);' \
        >/dev/null 2>&1
    test_assert signalled 143 $?
}
if "${PHP[@]}" -r 'exit(function_exists("posix_kill") ? 0 : 1);' 2>/dev/null; then
    test_fn=_signalled
else
    test_skip='ext/posix not available'
fi
test_need_ptrace=1
test_invoke

# outlives sampling: phpspy stops first, child exits later with its status
_outlives_sampling() {
    "$PHPSPY" --limit=0 --time-limit-ms=500 --quiet \
        -O/dev/null -E/dev/null -o /dev/null \
        -- "${PHP[@]}" -r 'usleep(1500000); exit(7);' >/dev/null 2>&1
    test_assert outlives_sampling 7 $?
}
test_fn=_outlives_sampling
test_need_ptrace=1
test_use_timeout_s=10
test_invoke

# bad -E path: error names the path; child not probed as a zombie
_bad_stderr_path() {
    local err
    err=$(
        "$PHPSPY" --limit=0 -O/dev/null -E/nonexistent-dir/x.err -o /dev/null \
            -- "${PHP[@]}" -r 'usleep(100000);' 2>&1 >/dev/null
    )
    test_assert_re bad_stderr_names_path 'nonexistent-dir/x\.err' "$err"
    test_assert bad_stderr_no_probe 0 "$(grep -c 'get_php_bin_path' <<<"$err")"
}
test_fn=_bad_stderr_path
test_need_ptrace=1
test_invoke

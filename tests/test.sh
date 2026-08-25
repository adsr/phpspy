#!/bin/bash

maybe_sudo=''
if [ -n "$need_ptrace" ]; then
    ptrace_scope=$(cat /proc/sys/kernel/yama/ptrace_scope 2>/dev/null || echo 0)
    if [ "$ptrace_scope" = "0" ]; then
        : # same-uid ptrace allowed; no privilege needed
    elif getcap "$PHPSPY" 2>/dev/null | grep -q cap_sys_ptrace; then
        : # binary has cap_sys_ptrace as a file cap
    elif sudo -n true &>/dev/null; then
        maybe_sudo='sudo -n'
    else
        skip='need ptrace'
    fi
fi

maybe_timeout=''
[ -n "$use_timeout_s" ] && maybe_timeout="timeout $use_timeout_s"

if [ -n "$skip" ]; then
    echo -e "  \x1b[33mSKIP\x1b[0m $skip"
    exit 0
fi

actual=$(
    $maybe_sudo $maybe_timeout $PHPSPY \
    --limit=1 \
    --child-stdout=/dev/null \
    --child-stderr=/dev/null \
    "${phpspy_opts[@]}" 2>test.err
)

exit_code=$?
actual_err=$(cat test.err 2>/dev/null)

if [ -n "$expected_exit_code" ]; then
    if [ "$exit_code" -eq "$expected_exit_code" ]; then
        echo -e "  \x1b[32mOK  \x1b[0m exit_code=$exit_code"
    else
        echo -e "  \x1b[31mERR \x1b[0m exit_code\nexpected=$expected_exit_code\n\nactual=$exit_code"
        cat test.err >&2
        exit 1
    fi
elif [ -z "$non_zero_ok" -a "$exit_code" -ne 0 ]; then
    echo -e "  \x1b[31mERR \x1b[0m exit_code=$exit_code"
    cat test.err >&2
    exit 1
fi

# check_re <testname> <regex> <haystack> <want_match: 1|0> <haystack label>
check_re() {
    if grep -Pq "$2" <<<"$3"; then
        found=1
    else
        found=0
    fi
    if [ "$found" -eq "$4" ]; then
        echo -e "  \x1b[32mOK  \x1b[0m $1"
    elif [ "$4" -eq 1 ]; then
        echo -e "  \x1b[31mERR \x1b[0m $1\nexpected=$2\n\n$5=\n$3"
        exit 1
    else
        echo -e "  \x1b[31mERR \x1b[0m $1\nnot_expected=$2\n\n$5=\n$3"
        exit 1
    fi
}

for testname in "${!expected[@]}"; do
    check_re "$testname" "${expected[$testname]}" "$actual" 1 actual
done

for testname in "${!not_expected[@]}"; do
    check_re "$testname" "${not_expected[$testname]}" "$actual" 0 actual
done

for testname in "${!expected_err[@]}"; do
    check_re "$testname" "${expected_err[$testname]}" "$actual_err" 1 stderr
done

for testname in "${!not_expected_err[@]}"; do
    check_re "$testname" "${not_expected_err[$testname]}" "$actual_err" 0 stderr
done

unset expected
unset not_expected
unset expected_err
unset not_expected_err
unset expected_exit_code
unset need_ptrace
unset use_timeout_s
unset skip
unset non_zero_ok

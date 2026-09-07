#!/bin/bash

test_init() {
    IFS=' ' read -ra PHP <<<"$PHP"
    : "${PHP[@]}"
}

test_assert() {
    local name="$1" exp="$2" observed="$3"
    if [ "$observed" = "$exp" ]; then
        echo -e "  \x1b[32mOK  \x1b[0m $name"
    else
        echo -e "  \x1b[31mERR \x1b[0m $name\nexpected=$exp\n\nactual=\n$observed"
        exit 1
    fi
}

test_assert_re() {
    local name="$1" expected_re="$2" observed="$3"
    if grep -Pq "$expected_re" <<<"$observed"; then
        echo -e "  \x1b[32mOK  \x1b[0m $name"
    else
        echo -e "  \x1b[31mERR \x1b[0m $name\nexpected=$expected_re\n\nactual=\n$observed"
        exit 1
    fi
}

test_invoke() {
    declare -gA test_expected test_not_expected
    declare -ga test_phpspy_opts

    local cmd_prefix=() actual exit_code testname

    if [ -n "$test_need_ptrace" ]; then
        local ptrace_scope
        ptrace_scope=$(cat /proc/sys/kernel/yama/ptrace_scope 2>/dev/null || echo 0)
        if [ "$ptrace_scope" = "0" ]; then
            :
        elif getcap "$PHPSPY" 2>/dev/null | grep -q cap_sys_ptrace; then
            :
        elif sudo -n true &>/dev/null; then
            cmd_prefix+=(sudo -n)
        else
            test_skip='need ptrace'
        fi
    fi
    [ -n "$test_use_timeout_s" ] && cmd_prefix+=(timeout "$test_use_timeout_s")

    if [ -n "$test_skip" ]; then
        echo -e "  \x1b[33mSKIP\x1b[0m $test_skip"
    elif [ -n "$test_fn" ]; then
        "$test_fn"
    else
        actual=$(
            "${cmd_prefix[@]}" "$PHPSPY" \
            --limit=1 \
            --child-stdout=/dev/null \
            --child-stderr=/dev/null \
            "${test_phpspy_opts[@]}" 2>test.err
        )
        exit_code=$?
        if [ -z "$test_non_zero_ok" ] && [ "$exit_code" -ne 0 ]; then
            echo -e "  \x1b[31mERR \x1b[0m exit_code=$exit_code"
            cat test.err >&2
            exit 1
        fi
        for testname in "${!test_expected[@]}"; do
            test_assert_re "$testname" "${test_expected[$testname]}" "$actual"
        done
        for testname in "${!test_not_expected[@]}"; do
            if ! grep -Pq "${test_not_expected[$testname]}" <<<"$actual"; then
                echo -e "  \x1b[32mOK  \x1b[0m $testname"
            else
                echo -e "  \x1b[31mERR \x1b[0m $testname\nnot_expected=${test_not_expected[$testname]}\n\nactual=\n$actual"
                exit 1
            fi
        done
    fi

    unset test_phpspy_opts test_expected test_not_expected test_need_ptrace test_use_timeout_s test_skip test_non_zero_ok test_fn
}

test_init

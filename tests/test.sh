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
    declare -gA expected not_expected
    declare -ga phpspy_opts

    local cmd_prefix=() actual exit_code testname

    if [ -n "$need_ptrace" ]; then
        local ptrace_scope
        ptrace_scope=$(cat /proc/sys/kernel/yama/ptrace_scope 2>/dev/null || echo 0)
        if [ "$ptrace_scope" = "0" ]; then
            :
        elif getcap "$PHPSPY" 2>/dev/null | grep -q cap_sys_ptrace; then
            :
        elif sudo -n true &>/dev/null; then
            cmd_prefix+=(sudo -n)
        else
            skip='need ptrace'
        fi
    fi
    [ -n "$use_timeout_s" ] && cmd_prefix+=(timeout "$use_timeout_s")

    if [ -n "$skip" ]; then
        echo -e "  \x1b[33mSKIP\x1b[0m $skip"
    elif [ -n "$test_fn" ]; then
        "$test_fn"
    else
        actual=$(
            "${cmd_prefix[@]}" "$PHPSPY" \
            --limit=1 \
            --child-stdout=/dev/null \
            --child-stderr=/dev/null \
            "${phpspy_opts[@]}" 2>test.err
        )
        exit_code=$?
        if [ -z "$non_zero_ok" ] && [ "$exit_code" -ne 0 ]; then
            echo -e "  \x1b[31mERR \x1b[0m exit_code=$exit_code"
            cat test.err >&2
            exit 1
        fi
        for testname in "${!expected[@]}"; do
            test_assert_re "$testname" "${expected[$testname]}" "$actual"
        done
        for testname in "${!not_expected[@]}"; do
            if ! grep -Pq "${not_expected[$testname]}" <<<"$actual"; then
                echo -e "  \x1b[32mOK  \x1b[0m $testname"
            else
                echo -e "  \x1b[31mERR \x1b[0m $testname\nnot_expected=${not_expected[$testname]}\n\nactual=\n$actual"
                exit 1
            fi
        done
    fi

    unset phpspy_opts expected not_expected need_ptrace use_timeout_s skip non_zero_ok test_fn
}

test_init

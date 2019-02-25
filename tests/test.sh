#!/bin/bash

maybe_sudo=''
[ -n "$use_sudo" ] && maybe_sudo='sudo -n'

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
    "${phpspy_opts[@]}"
)

exit_code=$?
if [ -z "$non_zero_ok" -a "$exit_code" -ne 0 ]; then
    echo -e "  \x1b[31mERR \x1b[0m exit_code=$exit_code"
    exit 1
fi

for testname in "${!expected[@]}"; do
    expected_re="${expected[$testname]}"
    if grep -Pq "$expected_re" <<<"$actual"; then
        echo -e "  \x1b[32mOK  \x1b[0m $testname"
    else
        echo -e "  \x1b[31mERR \x1b[0m $testname\nexpected=$expected_re\n\nactual=\n$actual"
        exit 1
    fi
done

for testname in "${!not_expected[@]}"; do
    not_expected_re="${not_expected[$testname]}"
    if ! grep -Pq "$not_expected_re" <<<"$actual"; then
        echo -e "  \x1b[32mOK  \x1b[0m $testname"
    else
        echo -e "  \x1b[31mERR \x1b[0m $testname\nnot_expected=$not_expected_re\n\nactual=\n$actual"
        exit 1
    fi
done

unset expected
unset not_expected
unset use_sudo
unset use_timeout_s
unset skip
unset non_zero_ok

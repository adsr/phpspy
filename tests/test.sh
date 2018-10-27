#!/bin/bash
[ -z "$phpv" ] && phpv=$($PHP -r '$v=PHP_VERSION; echo $v{0} . $v{2};')

maybe_sudo=''
[ -n "$use_sudo" ] && maybe_sudo='sudo'

maybe_timeout=''
[ -n "$use_timeout_s" ] && maybe_timeout="timeout $use_timeout_s"

actual=$(
    $maybe_sudo $maybe_timeout $PHPSPY \
    --limit=1 \
    --php-version=$phpv \
    --child-stdout=/dev/null \
    --child-stderr=/dev/null \
    "${phpspy_opts[@]}"
)

for testname in "${!expected[@]}"; do
    expected_re="${expected[$testname]}"
    if grep -Pq "$expected_re" <<<"$actual"; then
        echo -e "  \x1b[32mOK \x1b[0m $testname"
    else
        echo -e "  \x1b[31mERR\x1b[0m $testname\nexpected=$expected_re\n\nactual=\n$actual"
        exit 1
    fi
done

unset expected

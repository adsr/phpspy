#!/bin/bash
[ -z "$phpv" ] && phpv=$($PHP -r '$v=PHP_VERSION; echo $v{0} . $v{2};')

actual=$(
    $PHPSPY \
    --limit=1 \
    --php-version=$phpv\
    --child-stdout=/dev/null \
    --child-stderr=/dev/null \
    --request-info=qcup \
    $(eval "echo $phpspy_opts")
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

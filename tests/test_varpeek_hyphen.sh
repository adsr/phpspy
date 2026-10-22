#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck source=/dev/null
source "$TEST_SH"

php_file="$PWD/tests/varpeek-hyphen.php"
test_phpspy_opts=(--limit=0 --peek-var "a@$php_file:4" -- "${PHP[@]}" "$php_file")
declare -A test_expected
test_expected[varpeek_hyphenated_path]="^# varpeek a@$php_file:4 = 42$"
test_invoke

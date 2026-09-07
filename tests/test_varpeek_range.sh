#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck source=/dev/null
source "$TEST_SH"

read -r -d '' php_src <<'EOD'
<?php
function f() {
    $a = 1; $b = 2;
    sleep(1);
    $a = 3; $b = 4;
    sleep(1);
}
f();
EOD
php_file=$(mktemp)
echo "$php_src" >"$php_file"
test_phpspy_opts=(--limit=0 --peek-var "a@$php_file:4-7" --peek-var "b@$php_file:4-7" -- "${PHP[@]}" "$php_file")
declare -A test_expected
test_expected[varpeek_range_a_1]="^# varpeek a@$php_file:\d+ = 1"
test_expected[varpeek_range_b_2]="^# varpeek b@$php_file:\d+ = 2"
test_expected[varpeek_range_a_3]="^# varpeek a@$php_file:\d+ = 3"
test_expected[varpeek_range_b_4]="^# varpeek b@$php_file:\d+ = 4"
test_invoke
rm -f "$php_file"

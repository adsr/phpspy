#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck source=/dev/null
source "$TEST_SH"

read -r -d '' php_src <<'EOD'
<?php
function f() {
    $a = 42;
    sleep(1);
}
f();
EOD
php_file=$(mktemp)
echo "$php_src" >"$php_file"
phpspy_opts=(--limit=0 --peek-var "a@$php_file:4" -- "${PHP[@]}" "$php_file")
declare -A expected
expected[varpeek1       ]="^# varpeek a@$php_file:4 = 42"
test_invoke
rm -f "$php_file"

read -r -d '' php_src <<'EOD'
<?php
function f() {
    $a = ['k' => 42, 'j' => 'dolphin'];
    sleep(1);
}
f();
EOD
php_file=$(mktemp)
echo "$php_src" >"$php_file"
phpspy_opts=(--limit=0 --peek-var "a@$php_file:4" -- "${PHP[@]}" "$php_file")
declare -A expected
expected[varpeek2       ]="^# varpeek a@$php_file:4 = k=42,j=dolphin$"
test_invoke
rm -f "$php_file"

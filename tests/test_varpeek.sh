#!/bin/bash

read -r -d '' php_src <<'EOD'
<?php
function f() {
    $a = 42;
    sleep(1);
}
f();
EOD
php_file=$(mktemp)
echo "$php_src" >$php_file
phpspy_opts=(--limit=0 --peek-var "a@$php_file:4" -- $PHP $php_file)
declare -A expected
expected[varpeek        ]="^# varpeek a@$php_file:4 = 42"
source $TEST_SH
rm -f $php_file


read -r -d '' php_src <<'EOD'
<?php
function f() {
    $a = 1; $b = 2;
    sleep(1);
    $a = 3;
    $b = 4;
    sleep(1);
}
f();
EOD
php_file=$(mktemp)
echo "$php_src" >$php_file

phpspy_opts=(--limit=0 --peek-var "a@$php_file:4-7" --peek-var "b@$php_file:4-7"-- $PHP $php_file)
declare -A expected
expected[varpeek_range_a_line_4        ]="^# varpeek a@$php_file:4 = 1"
expected[varpeek_range_b_line_4        ]="^# varpeek b@$php_file:4 = 2"
expected[varpeek_range_a_line_7        ]="^# varpeek a@$php_file:7 = 3"
expected[varpeek_range_b_line_7        ]="^# varpeek b@$php_file:7 = 4"
source $TEST_SH


rm -f $php_file

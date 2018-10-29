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

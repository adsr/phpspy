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
phpspy_opts=(--limit=0 --peek-global "server.REQUEST_TIME" -- $PHP $php_file)
declare -A expected
expected[glopeek        ]="^# glopeek server.REQUEST_TIME = \d+"
source $TEST_SH
rm -f $php_file

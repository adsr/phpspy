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

export Ez=1 # These keys collide
export FY=2
phpspy_opts=(--limit=0 --peek-global "server.Ez" --peek-global "server.FY" -- $PHP -r "sleep(1);")
declare -A expected
expected[glopeek        ]="^# glopeek server.Ez = 1"
expected[glopeek        ]="^# glopeek server.FY = 2"
source $TEST_SH
rm -f $php_file

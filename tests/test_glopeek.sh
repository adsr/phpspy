#!/bin/bash

read -r -d '' php_src <<'EOD'
<?php
$GLOBALS['my_global'] = 'test_global';
$_SERVER['FAKE_VAR'] = 121;
$_SERVER['SCRIPT_NAME'] = 'not_real.php';
function f() {
    $a = 42;
    sleep(1);
}
f();
EOD
php_file=$(mktemp)
echo "$php_src" >$php_file
phpspy_opts=(--limit=0 --peek-global "server.REQUEST_TIME" --peek-global "globals.my_global" --peek-global "server.FAKE_VAR" --peek-global "server.SCRIPT_NAME" -- $PHP $php_file)
declare -A expected
expected[glopeek_server    ]="^# glopeek server.REQUEST_TIME = \d+"
expected[glopeek_globals   ]="^# glopeek globals.my_global = test_global"
expected[glopeek_server_new]="^# glopeek server.FAKE_VAR = 121"
expected[glopeek_server_mod]="^# glopeek server.SCRIPT_NAME = not_real.php"
source $TEST_SH
rm -f $php_file

export Ez=1 # These keys collide
export FY=2
phpspy_opts=(--limit=0 --peek-global "server.Ez" --peek-global "server.FY" -- $PHP -r "sleep(1);")
declare -A expected
expected[glopeek1       ]="^# glopeek server.Ez = 1"
expected[glopeek2       ]="^# glopeek server.FY = 2"
source $TEST_SH

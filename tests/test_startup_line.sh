#!/bin/bash

phpspy_opts=(-- $PHP -r 'usleep(2000000);')
declare -A expected_err
expected_err[startup        ]='^phpspy: pid \d+ php \S+ executor_globals=0x[0-9a-f]+ use_zend=[yn]$'
source $TEST_SH

# -q silences it
phpspy_opts=(--quiet -- $PHP -r 'usleep(2000000);')
declare -A not_expected_err
not_expected_err[no_startup  ]='executor_globals='
source $TEST_SH

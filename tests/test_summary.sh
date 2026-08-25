#!/bin/bash

phpspy_opts=(-- $PHP -r 'usleep(2000000);')
declare -A expected_err
expected_err[summary        ]='^phpspy: \d+ samples in \d+\.\d+s across \d+ pid'
expected_err[written        ]='\d+ written, \d+ empty, \d+ filtered'
expected_err[ms_per_sample  ]='\(\d+\.\d+ ms/sample\)$'
source $TEST_SH

# -q silences the summary
phpspy_opts=(--quiet -- $PHP -r 'usleep(2000000);')
declare -A not_expected_err
not_expected_err[no_summary  ]='samples in'
source $TEST_SH

# PHPSPY_NO_SUMMARY silences it too (this is what top mode uses)
PHPSPY_NO_SUMMARY=1
export PHPSPY_NO_SUMMARY
phpspy_opts=(-- $PHP -r 'usleep(2000000);')
declare -A expected
declare -A not_expected_err
expected[frame_0            ]='^0 usleep <internal>:-1$'
not_expected_err[no_summary  ]='samples in'
source $TEST_SH
unset PHPSPY_NO_SUMMARY

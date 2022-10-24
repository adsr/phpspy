#!/bin/bash

phpspy_opts=(--continue-on-error --buffer-size=24 -- $PHP -r 'usleep(1000000);')
declare -A expected
declare -A not_expected
expected[frame_0        ]='^0 usleep <internal>:-1$'
not_expected[no_frame_1 ]='^1 <main> <internal>:-1$'
source $TEST_SH

phpspy_opts=(--buffer-size=24 -- $PHP -r 'usleep(1000000);')
declare -A not_expected
not_expected[anything]='^.+$'
source $TEST_SH

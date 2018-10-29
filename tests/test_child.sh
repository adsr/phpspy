#!/bin/bash

phpspy_opts=(--request-info=qcup -- $PHP -r 'usleep(100000);')
declare -A expected
expected[frame_0        ]='^0 usleep <internal>:-1$'
expected[frame_1        ]='^1 <main> <internal>:-1$'
expected[req_uri        ]='^# uri = -$'
expected[req_path       ]='^# path = (Standard input code|-)$'
expected[req_qstring    ]='^# qstring = -$'
expected[req_cookie     ]='^# cookie = -$'
expected[req_ts         ]='^# ts = \d+\.\d+$'
source $TEST_SH

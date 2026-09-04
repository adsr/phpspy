#!/bin/bash

phpspy_opts=(--continue-on-error --buffer-size=24 -- $PHP -r 'usleep(2000000);')
declare -A expected
declare -A not_expected
expected[frame_0        ]='^0 usleep <internal>:-1$'
not_expected[no_frame_1 ]='^1 <main> <internal>:-1$'
source $TEST_SH

# Without --continue-on-error an over-budget trace used to be discarded
# entirely; it is now emitted truncated. A buffer this small leaves no room for
# the `# truncated = 1` marker itself (see test_truncate.sh for that).
phpspy_opts=(--buffer-size=24 -- $PHP -r 'usleep(2000000);')
declare -A expected
declare -A not_expected
expected[frame_0        ]='^0 usleep <internal>:-1$'
not_expected[no_frame_1 ]='^1 <main> <internal>:-1$'
source $TEST_SH

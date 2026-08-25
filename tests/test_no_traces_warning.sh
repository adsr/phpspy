#!/bin/bash

# A bogus executor_globals address makes every sample fail, which is the
# observable equivalent of a target that never executes PHP on the VM stack
# (e.g. one whose blocking calls are hooked by a coroutine extension).
# Child mode keeps phpspy the parent, so no ptrace privileges are needed.
phpspy_opts=(--limit=0 --time-limit-ms=2000 --rate-hz=5 --php-version=84 \
             --addr-executor-globals=1 --warn-no-traces-s=1 \
             -- $PHP -r 'usleep(3000000);')
declare -A expected_err
declare -A not_expected_err
expected_err[warn           ]='0 traces captured in 1s'
expected_err[warn_hint      ]='is the target executing PHP\?'
expected_err[summary_zero   ]='0 written'
expected_err[summary_warn   ]='captured 0 traces'
# A failed read must be reported as an error, not as "target wasn't executing
# PHP": both leave the depth at 0, but only one is a permissions/address
# problem, and conflating them misdiagnoses it.
expected_err[counted_errored]='[1-9]\d* errored'
not_expected_err[not_empty   ]='[1-9]\d* empty'
source $TEST_SH

# --warn-no-traces-s=0 disables the warning
phpspy_opts=(--limit=0 --time-limit-ms=2000 --rate-hz=5 --php-version=84 \
             --addr-executor-globals=1 --warn-no-traces-s=0 \
             -- $PHP -r 'usleep(3000000);')
declare -A not_expected_err
not_expected_err[no_warn     ]='0 traces captured in'
source $TEST_SH

# a healthy target must not warn
phpspy_opts=(--limit=0 --time-limit-ms=2000 --warn-no-traces-s=1 -- $PHP -r 'usleep(3000000);')
declare -A expected
declare -A not_expected_err
expected[frame_0            ]='^0 usleep <internal>:-1$'
not_expected_err[no_warn     ]='0 traces captured in'
source $TEST_SH

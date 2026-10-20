#!/bin/bash

if ! command -v pgrep >/dev/null 2>&1; then
    echo -e "  \x1b[33mSKIP\x1b[0m pgrep not available"
    exit 0
fi

# The -P string is word-split by the shell, so a multi-word pattern becomes
# several pgrep patterns and pgrep fails on every poll. phpspy used to loop on
# that forever while writing nothing.
err=$(timeout 15 $PHPSPY --limit=1 --pgrep '-f aaaaaaaa -f bbbbbbbb' --threads 1 2>&1 >/dev/null)
ec=$?

if [ "$ec" -eq 124 ]; then
    echo -e "  \x1b[31mERR \x1b[0m pgrep_failure_is_fatal (hung)"
    exit 1
fi
if grep -q 'exited with status 2' <<<"$err" && grep -q 'word-split' <<<"$err"; then
    echo -e "  \x1b[32mOK  \x1b[0m pgrep_failure_reported"
else
    echo -e "  \x1b[31mERR \x1b[0m pgrep_failure_reported\n$err"
    exit 1
fi
if [ "$ec" -ne 0 ]; then
    echo -e "  \x1b[32mOK  \x1b[0m pgrep_failure_exit_code (exit=$ec)"
else
    echo -e "  \x1b[31mERR \x1b[0m pgrep_failure_exit_code expected non-zero"
    exit 1
fi

#!/bin/bash

if ! command -v pgrep >/dev/null 2>&1; then
    echo -e "  \x1b[33mSKIP\x1b[0m pgrep not available"
    exit 0
fi

# A -P pattern nearly always matches phpspy's own command line, since the
# pattern text appears there. phpspy used to attach to itself (and to the shell
# and sudo above it), shelling out to objdump for each one on every poll.
err=$(timeout 15 $PHPSPY --limit=1 --pgrep '-f phpspy' --threads 2 --time-limit-ms=1000 2>&1 >/dev/null)

for pat in 'objdump' 'get_symbol_offset: Failed' 'get_php_bin_path: Failed'; do
    if grep -q "$pat" <<<"$err"; then
        echo -e "  \x1b[31mERR \x1b[0m no_self_probe: saw '$pat'\n$err"
        exit 1
    fi
done
echo -e "  \x1b[32mOK  \x1b[0m no_self_probe"

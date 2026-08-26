#!/bin/bash

# In child mode phpspy is a wrapper, so `$?` must be the command's own status.
# Written standalone rather than through test.sh, which has no exit-code
# assertion of its own.

check_exit() { # <label> <expected> <php args...>
    local label="$1" want="$2"; shift 2
    $PHPSPY --limit=0 --quiet -O/dev/null -E/dev/null -o /dev/null -- "$@" >/dev/null 2>&1
    local got=$?
    if [ "$got" -eq "$want" ]; then
        echo -e "  \x1b[32mOK  \x1b[0m $label (exit=$got)"
    else
        echo -e "  \x1b[31mERR \x1b[0m $label\nexpected=$want\n\nactual=$got"
        exit 1
    fi
}

check_exit exit_0   0   $PHP -r 'exit(0);'
check_exit exit_3   3   $PHP -r 'exit(3);'
check_exit exit_42  42  $PHP -r 'exit(42);'

# a child killed by a signal reports 128+signum
if $PHP -r 'exit(function_exists("posix_kill") ? 0 : 1);' 2>/dev/null; then
    check_exit signalled 143 $PHP -r 'posix_kill(posix_getpid(), SIGTERM); usleep(5000000);'
else
    echo -e "  \x1b[33mSKIP\x1b[0m signalled (ext/posix not available)"
fi

# a target that outlives sampling still reports its own status
$PHPSPY --limit=0 --time-limit-ms=500 --quiet -O/dev/null -E/dev/null -o /dev/null \
    -- $PHP -r 'usleep(1500000); exit(7);' >/dev/null 2>&1
got=$?
if [ "$got" -eq 7 ]; then
    echo -e "  \x1b[32mOK  \x1b[0m outlives_sampling (exit=$got)"
else
    echo -e "  \x1b[31mERR \x1b[0m outlives_sampling\nexpected=7\n\nactual=$got"
    exit 1
fi

# an unwritable -E must name the path, and must not then probe a zombie
err=$($PHPSPY --limit=0 -O/dev/null -E/nonexistent-dir/x.err -o /dev/null \
    -- $PHP -r 'usleep(100000);' 2>&1 >/dev/null)
if grep -q "nonexistent-dir/x.err" <<<"$err" && ! grep -q 'get_php_bin_path' <<<"$err"; then
    echo -e "  \x1b[32mOK  \x1b[0m names_unwritable_path"
else
    echo -e "  \x1b[31mERR \x1b[0m names_unwritable_path\n$err"
    exit 1
fi

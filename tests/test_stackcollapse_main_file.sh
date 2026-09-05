#!/bin/bash

REPO=$(dirname $(dirname $TEST_SH))

on_exit() { rm -rf $tmpdir; }
fail()    { echo -e "  \x1b[31mERR \x1b[0m $@"; [ -n "$collapsed" ] && echo -e "\ncollapsed=\n$collapsed"; exit 1; }

tmpdir=$(mktemp -d)
trap on_exit EXIT

# Three files, each contributing a `<main>` frame to the same stack. The
# innermost is included from inside a method, so PHP names it `A::<main>`.
cat >$tmpdir/entry.php <<'EOF'
<?php
require __DIR__ . '/inc.php';
EOF
cat >$tmpdir/inc.php <<'EOF'
<?php
class A {
    public function m() {
        require __DIR__ . '/method_inc.php';
    }
}
(new A())->m();
EOF
cat >$tmpdir/method_inc.php <<'EOF'
<?php
sleep(2);
EOF

$PHPSPY -O/dev/null -E/dev/null -o $tmpdir/traces 2>/dev/null \
    -- $PHP $tmpdir/entry.php
grep -q '<main>' $tmpdir/traces || fail "Failed to capture traces"

collapsed=$($REPO/stackcollapse-phpspy.pl $tmpdir/traces)
grep -q '<main>:entry\.php;<main>:inc\.php;' <<<"$collapsed" \
    || fail "Nested <main> frames not disambiguated by file"
grep -q 'A::<main>:method_inc\.php' <<<"$collapsed" \
    || fail "Class-scoped <main> frame not disambiguated by file"
echo -e "  \x1b[32mOK  \x1b[0m main_file"

collapsed=$($REPO/stackcollapse-phpspy.pl --no-main-file $tmpdir/traces)
grep -q '<main>;<main>;' <<<"$collapsed" \
    || fail "--no-main-file did not restore bare <main> frames"
grep -q '<main>:' <<<"$collapsed" \
    && fail "--no-main-file appended a file to a <main> frame"
echo -e "  \x1b[32mOK  \x1b[0m no_main_file"

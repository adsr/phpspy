#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck source=/dev/null
source "$TEST_SH"

repo=$(dirname "$(dirname "$TEST_SH")")
collapse="$repo/stackcollapse-phpspy.pl"

# Canned trace covering the labelling rules: shortest unique suffix for
# same-named files, class prefix dropped, no file for <internal>, `;` and
# whitespace replaced, trailing slashes ignored, comment lines not counted.
read -r -d '' canned_in <<'EOD'
0 sleep <internal>:-1
1 <main> /var/www/public/index.php:3
# - - -
0 sleep <internal>:-1
1 <main> /var/www/admin/index.php:3
# - - -
0 sleep <internal>:-1
1 <main> /var/www/my app/index.php:3
# - - -
0 sleep <internal>:-1
1 A::<main> /var/www/lib/inc.php:1
2 A::m /var/www/public/index.php:9
3 <main> /var/www/public/index.php:12
# - - -
0 sleep <internal>:-1
1 <main> <internal>:-1
# - - -
0 sleep <internal>:-1
1 <main> /var/www/semi;colon.php:1
# - - -
0 sleep <internal>:-1
1 <main> /var/www/trail.php/:1
# - - -
0 sleep <internal>:-1
1 <main> /var/www/trail.php//:1
# - - -
# <main> /zzz/plain.php:1
0 sleep <internal>:-1
1 <main> /var/www/plain.php:1
EOD
read -r -d '' canned_out <<'EOD'
<main>:admin/index.php;sleep 1
<main>:my_app/index.php;sleep 1
<main>:plain.php;sleep 1
<main>:public/index.php;A::m;<main>:inc.php;sleep 1
<main>:public/index.php;sleep 1
<main>:semi_colon.php;sleep 1
<main>:trail.php;sleep 2
<main>;sleep 1
EOD
_canned() {
    test_assert main_file_canned "$canned_out" "$("$collapse" <<<"$canned_in" | LC_ALL=C sort)"
}
test_fn=_canned
test_invoke

# Live trace: three files, each contributing a `<main>` frame to the same
# stack. The innermost is included from inside a method, two of the files
# share a basename, and the directory name has a space.
tmp_dir=$(mktemp -d)
_exit() { rm -rf "$tmp_dir"; }
trap _exit EXIT
php_dir="$tmp_dir/my app"
mkdir -p "$php_dir/sub"
read -r -d '' php_src <<'EOD'
<?php
require __DIR__ . '/inc.php';
EOD
echo "$php_src" >"$php_dir/entry.php"
read -r -d '' php_src <<'EOD'
<?php
class A {
    public function m() {
        require __DIR__ . '/sub/inc.php';
    }
}
(new A())->m();
EOD
echo "$php_src" >"$php_dir/inc.php"
read -r -d '' php_src <<'EOD'
<?php
sleep(1);
EOD
echo "$php_src" >"$php_dir/sub/inc.php"

_live() {
    local collapsed
    collapsed=$(
        "$PHPSPY" -O/dev/null -E/dev/null 2>"$tmp_dir/phpspy.err" \
            -- "${PHP[@]}" "$php_dir/entry.php" \
            | "$collapse"
    )
    [ -n "$collapsed" ] || cat "$tmp_dir/phpspy.err" >&2
    test_assert_re main_file_live \
        '^<main>:entry\.php;<main>:my_app/inc\.php;A::m;<main>:sub/inc\.php;sleep \d+$' "$collapsed"
    test_assert main_file_no_bare 0 "$(grep -cP '<main>(?!:)' <<<"$collapsed")"
    test_assert main_file_no_scope_prefix 0 "$(grep -c '::<main>' <<<"$collapsed")"
}
test_fn=_live
test_invoke

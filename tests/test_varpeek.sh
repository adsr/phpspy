#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck source=/dev/null
source "$TEST_SH"

read -r -d '' php_src <<'EOD'
<?php
function f() {
    $a = 42;
    sleep(1);
}
f();
EOD
php_file=$(mktemp)
echo "$php_src" >"$php_file"
test_phpspy_opts=(--limit=0 --peek-var "a@$php_file:4" -- "${PHP[@]}" "$php_file")
declare -A test_expected
test_expected[varpeek1       ]="^# varpeek a@$php_file:4 = 42$"
test_invoke
rm -f "$php_file"

read -r -d '' php_src <<'EOD'
<?php
function f() {
    $a = ['k' => 42, 'j' => 'dolphin'];
    sleep(1);
}
f();
EOD
php_file=$(mktemp)
echo "$php_src" >"$php_file"
test_phpspy_opts=(--limit=0 --peek-var "a@$php_file:4" -- "${PHP[@]}" "$php_file")
declare -A test_expected
test_expected[varpeek2       ]="^# varpeek a@$php_file:4 = k=42,j=dolphin$"
test_invoke
rm -f "$php_file"

peek_length=5000
peek_buffer_size=8192
php_src="<?php
function f() {
    \$a = str_repeat('x', $peek_length);
    sleep(1);
}
f();"
php_file=$(mktemp)
echo "$php_src" >"$php_file"
test_phpspy_opts=(--limit=0 --buffer-size "$peek_buffer_size" --peek-max-len "$peek_length" --peek-var "a@$php_file:4" -- "${PHP[@]}" "$php_file")
declare -A test_expected
test_expected[varpeek_long]="^# varpeek a@$php_file:4 = x{$peek_length}$"
test_invoke
rm -f "$php_file"

help_output=$("$PHPSPY" --help)
test_assert_re peek_max_len_help "peek-max-len=<len>" "$help_output"

virtual_memory_limit_kib=131072
oversized_peek_length=268435456
allocation_error=$(
    ulimit -v "$virtual_memory_limit_kib"
    "$PHPSPY" \
        --peek-max-len "$oversized_peek_length" \
        --limit=1 \
        --child-stdout=/dev/null \
        --child-stderr=/dev/null \
        -- "${PHP[@]}" -r 'sleep(1);' 2>&1
)
test_assert_re peek_max_len_allocation_error "^main_pid: malloc peek buffer:" "$allocation_error"

read -r -d '' php_src <<'EOD'
<?php
function f() {
    $a = 0;
    sleep(1);
    $a = 1;
    sleep(1);
    $a = 2;
    sleep(1);
}
f();
EOD
php_file=$(mktemp -t with-hyphen.XXXXXXXXXX) # hyphen in fname should not be
echo "$php_src" >"$php_file"                 # mistaken for line range
test_phpspy_opts=(--limit=0 --peek-var "a@$php_file:6" -- "${PHP[@]}" "$php_file")
declare -A test_expected
declare -A test_not_expected
test_expected[varpeek_hyphen_a_1            ]="^# varpeek a@$php_file:\d+ = 1$"
test_not_expected[varpeek_hyphen_a_no_0_or_2]="^# varpeek a@$php_file:\d+ = [02]$"
test_invoke
rm -f "$php_file"

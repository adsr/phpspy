#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck source=/dev/null
source "$TEST_SH"

repo=$(dirname "$(dirname "$TEST_SH")")

_run_fixtures() {
    local out
    # two traces whose chunks interleave: A.0 B.0 A.1 B.1
    out=$("$repo/stackcollapse-phpspy.pl" <<'EOF' | sort
0 aaa /t.php:1
1 bbb /t.php:2
# trace_id = 1.0/2
0 ccc /t.php:1
1 ddd /t.php:2
# trace_id = 2.0/2
2 <main> /t.php:3
# trace_id = 1.1/2

3 <main> /t.php:4
# trace_id = 2.1/2

EOF
)
    test_assert "interleaved_chunks" "$(printf '<main>;bbb;aaa 1\n<main>;ddd;ccc 1')" "$out"

    # pre-trace_id capture files still collapse
    out=$("$repo/stackcollapse-phpspy.pl" <<'EOF' | sort
0 aaa /t.php:1
1 <main> /t.php:2

0 aaa /t.php:1
1 <main> /t.php:2

EOF
)
    test_assert "legacy_no_markers" "<main>;aaa 2" "$out"

    # a chunk arriving out of order is discarded, later traces are unaffected
    out=$("$repo/stackcollapse-phpspy.pl" 2>/dev/null <<'EOF'
0 aaa /t.php:1
# trace_id = 9.1/2
0 zzz /t.php:1
1 <main> /t.php:2
# trace_id = 8.0/1

EOF
)
    test_assert "out_of_order_chunk_discarded" "<main>;zzz 1" "$out"
}
test_fn=_run_fixtures
test_invoke

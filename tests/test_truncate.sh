#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck disable=SC2016 # ignore $var in single quote strings
# shellcheck source=/dev/null
source "$TEST_SH"

long_fn="f$(printf 'x%.0s' $(seq 1 200))"

# one frame line is longer than a 128-byte chunk's 64-byte payload
_run_single_record() {
    local raw out rc blanks depth0
    # capture to a file: command substitution would eat the trailing blank line
    raw=$(mktemp)
    # --limit=5: only the child's startup sample can avoid truncation, so if a
    # BUF_FULL sample did not count toward the limit this would run until the
    # timeout killed it
    timeout 10 "$PHPSPY" --limit=5 --child-stdout=/dev/null --child-stderr=/dev/null \
          --buffer-size=128 -- "${PHP[@]}" -r "function ${long_fn}(){ usleep(2000000); } ${long_fn}();" \
          >"$raw" 2>/dev/null
    rc=$?
    out=$(cat "$raw")
    depth0=$(grep -c '^0 ' "$raw")
    blanks=$(grep -c '^$' "$raw")
    rm -f "$raw"
    test_assert "trunc_limit_terminates" "0" "$rc"        # BUF_FULL sample counted toward --limit
    test_assert_re "trunc_marker" '^# truncated = 1$' "$out"
    test_assert_re "trunc_chunk_marker" '^# trace_id = \d+\.\d+/\d+$' "$out"
    test_assert "trunc_one_delim_per_trace" "$depth0" "$blanks"   # delimiter guarantee
}
test_fn=_run_single_record
test_invoke

# single-line mode: chunking disabled, one line per trace, marked truncated.
# NOTE: in -1 mode the tab is the *trailing* record delimiter; lines still start with "0 ".
_run_single_line() {
    local out lines depth0
    out=$(timeout 10 "$PHPSPY" --limit=0 --time-limit-ms=1000 \
          --child-stdout=/dev/null --child-stderr=/dev/null -1 --buffer-size=128 \
          -- "${PHP[@]}" -r 'function f($n){ if($n) f($n-1); else usleep(2000000); } f(20);' 2>/dev/null)
    lines=$(grep -c . <<<"$out")
    depth0=$(grep -c '^0 ' <<<"$out")
    test_assert "single_line_one_line_per_trace" "$lines" "$depth0"
    test_assert_re "single_line_truncated" '# truncated = 1' "$out"
    test_assert_re "single_line_marker" '# trace_id = \d+\.0/1' "$out"
}
test_fn=_run_single_line
test_invoke

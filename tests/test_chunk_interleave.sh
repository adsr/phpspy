#!/bin/bash
# shellcheck disable=SC2034 # ignore seemingly unused test_invoke params
# shellcheck disable=SC2016 # ignore $var in single quote strings
# shellcheck disable=SC2154 # test_cmd_prefix is assigned by the sourced test.sh
# shellcheck source=/dev/null
source "$TEST_SH"

repo=$(dirname "$(dirname "$TEST_SH")")

_run_interleave() {
    local a_pid b_pid collapsed mixed
    # The token lives in each child's argv (inside a PHP comment). The pgrep pattern
    # spells it with a bracket so that neither phpspy's own argv nor the `sh -c pgrep ...`
    # child (both of which contain the bracketed form) matches itself.
    "${PHP[@]}" -r '/*phpspy_ilv_token*/ function aaaa_f($n){ if($n) aaaa_f($n-1); else usleep(3000000); } aaaa_f(40);' >/dev/null 2>&1 &
    a_pid=$!
    "${PHP[@]}" -r '/*phpspy_ilv_token*/ function bbbb_f($n){ if($n) bbbb_f($n-1); else usleep(3000000); } bbbb_f(40);' >/dev/null 2>&1 &
    b_pid=$!
    sleep 0.3
    collapsed=$("${test_cmd_prefix[@]}" "$PHPSPY" -P '-f phpspy_ilv_toke[n]' --threads 2 \
        --buffer-size=512 --time-limit-ms=2000 --limit=0 2>/dev/null \
        | "$repo/stackcollapse-phpspy.pl" 2>/dev/null)
    wait "$a_pid" "$b_pid"
    test_assert_re "interleave_saw_aaaa" '(^|;)aaaa_f' "$collapsed"
    test_assert_re "interleave_saw_bbbb" '(^|;)bbbb_f' "$collapsed"
    mixed=$(grep -cP '^(?=.*aaaa_f)(?=.*bbbb_f)' <<<"$collapsed")
    test_assert "interleave_no_mixed_stacks" "0" "$mixed"
    test_assert "interleave_no_partial_stacks" "0" "$(grep -cv '^<main>' <<<"$collapsed")"
}
test_need_ptrace=1
test_use_timeout_s=15
test_fn=_run_interleave
test_invoke

# round-trip: no sample lost, no stack cut short
_run_roundtrip() {
    local raw collapsed n_traces n_collapsed
    raw=$(mktemp)
    timeout 10 "$PHPSPY" --limit=0 --time-limit-ms=2000 --buffer-size=256 \
        --child-stdout=/dev/null --child-stderr=/dev/null \
        -- "${PHP[@]}" -r 'function f($n){ if($n) f($n-1); else usleep(3000000); } f(40);' >"$raw" 2>/dev/null
    collapsed=$("$repo/stackcollapse-phpspy.pl" <"$raw")
    n_traces=$(grep -c '^0 ' "$raw")
    n_collapsed=$(awk '{s+=$NF} END{print s+0}' <<<"$collapsed")
    rm -f "$raw"
    test_assert "roundtrip_no_sample_lost" "$n_traces" "$n_collapsed"
    test_assert "roundtrip_no_partial_stacks" "0" "$(grep -cv '^<main>' <<<"$collapsed")"
}
test_fn=_run_roundtrip
test_invoke

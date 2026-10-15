# Patch an existing structs/{arch}/php_structs_NN.h mirror header's field
# offsets (and the padN[] sizes that precede them) to match a fresh
# struct_dump.gdb probe, WITHOUT re-deriving struct layout from scratch.
#
# Scope, deliberately narrow: this only fixes the shape of drift actually
# observed across every PHP version and both architectures so far -- a
# single named field's absolute offset moving, with an immediately
# preceding `uint8_t padN[...]` line able to absorb the delta. Everything
# outside that shape is left untouched and reported on stderr for a human
# to look at; this script never invents new padding layouts, never adds or
# removes fields, and never touches union blocks (zend_function, the zval
# u1/u2 members) at all -- those are flagged only.
#
# Why this is safe without tracking a running cursor: every field's offset
# comment in these headers is an ABSOLUTE byte offset from the start of its
# own struct, not a cumulative sum of preceding fields. So each field can be
# checked and patched independently against the fresh dump, in isolation,
# regardless of what else in the struct did or didn't change.
#
# Line format (verified against the checked-in headers): 4-space indent,
# then the C type left-justified in a 24-char field, then the
# name+decorators+semicolon left-justified in a 24-char field, then
# "/* " + offset left-justified in a 9-char field + "+" + size + " */".
# Reconstructing lines with this exact rule (rather than in-place text
# substitution) is what keeps column alignment correct when a number's
# digit count changes.
#
# Usage:
#   gawk -f patch_struct_header.awk -v dumpfile=OFFSETS.txt HEADER.h > HEADER.h.new
#
# OFFSETS.txt is the output of parse_struct_dump.awk: lines of
# "Type.field offset".
#
# Exit status is nonzero if anything needed human review (see stderr).

BEGIN {
    if (dumpfile == "") {
        print "patch_struct_header.awk: -v dumpfile=... is required" > "/dev/stderr"
        exit 2
    }
    while ((getline dline < dumpfile) > 0) {
        split(dline, dparts, " ")
        dump_off[dparts[1]] = dparts[2]
    }
    close(dumpfile)

    in_block = 0
    needs_review = 0
}

match($0, /^(struct|union) __attribute__\(\(__packed__\)\) _([A-Za-z_]+)_([0-9]+) \{$/, m) {
    in_block = 1
    is_union = (m[1] == "union")
    type_key = m[2]
    nblock = 0
    print
    next
}

in_block && /^\};$/ {
    process_block()
    in_block = 0
    print
    next
}

in_block {
    nblock++
    line[nblock] = $0
    typecol[nblock] = substr($0, 5, 24)
    namecol[nblock] = substr($0, 29, 24)

    if (match($0, /^    uint8_t {17}pad[0-9]+\[([0-9]+)\]; *\/\* *([0-9]+) *\+([0-9]+) \*\/$/, pm)) {
        kind[nblock] = "pad"
        pad_size[nblock] = pm[1] + 0
        off[nblock] = pm[2] + 0
        next
    }

    if (match($0, /\/\* *([0-9]+) *\+([0-9]+) \*\/$/, fm)) {
        # Named field. Extract NAME from namecol: strip a leading `*`,
        # trailing `;` and any `[N]` array suffix.
        nm = namecol[nblock]
        gsub(/^ */, "", nm)
        sub(/^\*/, "", nm)
        sub(/\[[0-9]+\]/, "", nm)
        sub(/;.*$/, "", nm)
        kind[nblock] = "field"
        fname[nblock] = nm
        off[nblock] = fm[1] + 0
        size[nblock] = fm[2] + 0
        next
    }

    kind[nblock] = "other"
    next
}

# Anything not inside a block (typedefs, blank lines, #ifndef/#endif, ...)
# is passed through completely unchanged.
{ print }

END {
    if (needs_review) exit 1
}

function render(type_text, name_text, o, s) {
    return sprintf("    %-24s%-24s/* %-9d+%d */", type_text, name_text, o, s)
}

function process_block(   i, key, want, delta, newpad, prev_is_pad, nametext) {
    for (i = 1; i <= nblock; i++) {
        if (kind[i] != "field") continue
        key = type_key "." fname[i]
        if (!(key in dump_off)) continue
        want = dump_off[key] + 0
        if (want == off[i]) continue

        if (is_union) {
            printf("patch_struct_header: %s union field %s offset differs (header=%d dump=%d) -- union blocks are never auto-patched, review by hand\n", type_key, fname[i], off[i], want) > "/dev/stderr"
            needs_review = 1
            continue
        }

        prev_is_pad = (i > 1 && kind[i-1] == "pad")
        if (!prev_is_pad) {
            printf("patch_struct_header: %s.%s offset differs (header=%d dump=%d) but has no immediately preceding pad to absorb it -- review by hand\n", type_key, fname[i], off[i], want) > "/dev/stderr"
            needs_review = 1
            continue
        }

        delta = want - off[i]
        newpad = pad_size[i-1] + delta
        if (newpad < 0) {
            printf("patch_struct_header: %s.%s would need a negative pad (header=%d dump=%d, preceding pad=%d) -- review by hand\n", type_key, fname[i], off[i], want, pad_size[i-1]) > "/dev/stderr"
            needs_review = 1
            continue
        }

        nametext = namecol[i-1]
        sub(/\[[0-9]+\]/, "[" newpad "]", nametext)
        line[i-1] = render(typecol[i-1], nametext, off[i-1], newpad)
        pad_size[i-1] = newpad

        line[i] = render(typecol[i], namecol[i], want, size[i])

        printf("patch_struct_header: %s.%s %d -> %d (preceding pad %d -> %d)\n", type_key, fname[i], off[i], want, pad_size[i-1] - delta, pad_size[i-1]) > "/dev/stderr"
        off[i] = want
    }
    for (i = 1; i <= nblock; i++) print line[i]
}

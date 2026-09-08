# Normalize struct_dump.gdb output into flat "Type.field offset" lines.
#
# Usage: awk -f parse_struct_dump.awk struct_dump.SOMEVERSION.out
#
# Input looks like:
#   zend_array
#   type = struct _zend_array
#     type = uint32_t
#     nTableMask 12 +4
#     ...
#
# Output: one line per named field, "Type.field offset", sorted by nothing
# in particular (caller's problem). Anonymous union members (u1, u2,
# common) are dropped -- they're structural, not addressable fields, and
# phpspy_trace.c never reads them directly.

/^[a-zA-Z_]+$/ { s = $1; next }
/type =/ { next }
/^  [A-Za-z_.]+ [0-9]+ \+[0-9]+$/ {
    f = $1
    n = split(f, p, ".")
    f = p[n]
    if (f == "u1" || f == "u2" || f == "common") next
    print s "." f, $2
}

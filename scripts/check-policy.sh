#!/usr/bin/env bash
# Mechanical checks for rules from AGENTS.md that grep can catch.
# Run from the repository root. Exits non-zero on violation.
set -u

status=0
src_globs=(--include='*.c' --include='*.cpp' --include='*.h' --include='*.hpp'
           --include='*.S' --include='*.s' --include='*.inc')

# Rule 6: EL2 is reserved. The only permitted EL2 code is the boot-time
# drop from EL2 to EL1 in src/aarch64/start.c.
el2_allow='^src/aarch64/start\.c:'
el2_hits=$(grep -rnIE "${src_globs[@]}" \
    -e '\b[A-Z0-9_]+_EL2\b' \
    -e '\bhvc\b' \
    -e '__builtin_arm_hvc' \
    src include 2>/dev/null | grep -vE "$el2_allow" | grep -v '^src/attic/')

if [ -n "$el2_hits" ]; then
    echo "::error::EL2/hypervisor use outside the boot path (AGENTS.md rule 6: EL2 is reserved for the planned MMU design):"
    echo "$el2_hits"
    status=1
fi

# Policy files must exist and CLAUDE.md must still import AGENTS.md.
for f in AGENTS.md CONTRIBUTING.md CLAUDE.md; do
    [ -f "$f" ] || { echo "::error::$f is missing"; status=1; }
done
grep -q '@AGENTS.md' CLAUDE.md 2>/dev/null || { echo "::error::CLAUDE.md must import @AGENTS.md"; status=1; }

[ $status -eq 0 ] && echo "Policy check passed."
exit $status

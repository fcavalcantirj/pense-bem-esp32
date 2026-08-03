#!/usr/bin/env bash
# run.sh — host proof harness for devices/esp32-pense-bem/pensebem.h
#
#   bash devices/esp32-pense-bem/formula-test/run.sh
#
# SEVEN STAGES RUN. Green here means the formula and the port are both proven
# BEFORE anything is flashed and before a single wire is plugged in.
#
# ⚠ WHY TWO FIXTURES AND NOT ONE — this is the whole design, and collapsing them
#   would make the suite theatre:
#
#     fixtures/real-books.tsv     rows typed off REAL PRINTED TOYS by the upstream
#                                 reverse-engineers. Independent of our code.
#                                 -> proves THE FORMULA IS RIGHT.
#                                 -> cannot prove coverage: 925 of 14850 cells.
#
#     fixtures/reference-table.txt  the upstream generator's own output, committed.
#                                 -> proves THE PORT IS RIGHT, all 99x150.
#                                 -> ⚠ CIRCULAR. It was produced BY the formula it
#                                    would be vouching for. Stage 1 proves this on
#                                    the spot: the file literally BEGINS with the
#                                    seed constant.
#
#   The rule: an assertion is vacuous exactly when its two sides SHARE A TERM.
#   Checking generated output against generated output shares the term "the
#   reverse-engineered formula" and co-varies into a tautology. Only the harvested
#   rows can falsify the formula; only the table can cover the port. Neither is
#   redundant and neither substitutes for the other.
set -euo pipefail
cd "$(dirname "$0")"

CC=${CC:-cc}
BIN=./pb-formula
HDR=../pensebem.h
INO=../pense-bem-esp32.ino
BOOKS=fixtures/real-books.tsv
TABLE=fixtures/reference-table.txt

red()  { printf '\033[31m%s\033[0m\n' "$*"; }
grn()  { printf '\033[32m%s\033[0m\n' "$*"; }
# ⚠ die() WRITES TO STDERR, AND THAT IS LOAD-BEARING. mutant() is called inside
#   $( ), so a die() on stdout would be CAPTURED INTO THE VARIABLE instead of
#   shown, and `exit 1` would only leave the subshell — the "mutation did not
#   apply" guard would fail SILENTLY, which is precisely what it exists to
#   prevent. Caught by deliberately reformatting the header on 2026-08-02.
die()  { printf '\033[31mFAIL: %s\033[0m\n' "$*" >&2; exit 1; }

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# Builds a mutant from a sed expression, refusing if the mutation did not apply.
# ⚠ A MUTATION THAT DOES NOT APPLY IS NOT A RED-ON-DEMAND PROOF. The better-known
#   version of this hole is "a mutation that does not compile is not a proof";
#   it exists one step earlier too — a sed whose pattern stopped
#   matching because the header was reformatted mutates NOTHING, and the control
#   then goes green while reporting itself as red.
mutant() {
  # ⚠ Separate `local` statements: macOS ships bash 3.2, where a later assignment
  #   in the SAME `local` cannot see an earlier one, and `set -u` then aborts.
  local name="$1"
  local expr="$2"
  local dir="$work/$name"
  mkdir -p "$dir/formula-test"
  sed "$expr" "$HDR" > "$dir/pensebem.h"
  cmp -s "$HDR" "$dir/pensebem.h" && die "mutation '$name' DID NOT APPLY — its sed pattern no longer matches the header"
  cp pb-formula.c "$dir/formula-test/pb-formula.c"
  ( cd "$dir/formula-test" && $CC -std=c99 -O0 -w -o pb-formula pb-formula.c ) \
    || die "mutant '$name' did not compile — that is not a proof either"
  echo "$dir/formula-test/pb-formula"
}

field() { grep "^$1=" | cut -d= -f2; }

echo "== 1. DRIFT + PROVENANCE GUARD =="

grep -qF '#include "pensebem.h"' "$INO" 2>/dev/null \
  || echo "   (note: $INO not written yet — the include guard will apply once it is)"

# ⚠ THE ANTI-COPY GUARD. The instant anyone pastes the constants into the sketch,
#   "this test reads the shipped code" becomes a lie. Nothing else catches that.
#   fixtures/ is excluded on purpose — see the circularity proof just below.
#   ⚠ The probe is built from two halves so the literal never appears contiguously
#     in THIS file — otherwise the guard trips on itself, which it correctly did
#     the first time it ran. -I skips the compiled binary, which links the
#     constant in and would otherwise trip it too.
PROBE="dbaadcbdaad""cbbc"
copies=$(grep -rlIF "$PROBE" .. --exclude-dir=fixtures --exclude-dir=build --exclude=run.sh 2>/dev/null | sort)
[ "$copies" = "../pensebem.h" ] \
  || die "the seed constant must live in pensebem.h and NOWHERE else. Found in:
$copies"
grn "   seed constant lives in pensebem.h only"

# ⚠ CIRCULARITY, PROVEN ON THE SPOT rather than asserted in a comment: the
#   reference table's first 150 characters ARE the seed constant. A file that
#   embeds the input of the formula cannot be independent evidence for it.
head -c 15 "$TABLE" | grep -qF "$PROBE" \
  || die "reference table no longer begins with the seed — re-check its provenance"
grn "   reference table BEGINS with the seed constant -> circular, port-only evidence"

# ⚠ Comments are stripped first. The header's own banner NAMES the things it
#   forbids ("NO <Arduino.h>. NO String."), so grepping the raw file trips on the
#   documentation instead of on the code — which it did the first time this ran.
code_only() { sed -e 's,//.*,,' -e '/^[[:space:]]*\*/d' -e '/^[[:space:]]*\/\*/d' "$1"; }

for banned in '<Arduino.h>' 'String ' 'Serial\.' 'millis(' 'pinMode' 'digitalWrite' 'stdbool.h' 'malloc(' 'float ' 'double '; do
  code_only "$HDR" | grep -q -- "$banned" && die "pensebem.h must stay Arduino-free; found: $banned"
done
grn "   pensebem.h is dependency-free (compiles under both toolchains)"

# The driver must own NO arithmetic, or it stops being a harness and becomes a mirror.
for banned in "'a' +" '% 4' '& 3' 'pb_chained_offsets\['; do
  code_only pb-formula.c | grep -q -- "$banned" && die "pb-formula.c must own no algorithm; found: $banned"
done
grn "   pb-formula.c owns no algorithm"

# ⚠ FIXTURE COUNTS PINNED AS LITERALS, BEFORE ANYTHING COMPILES. A fixture that
#   gets "cleaned up" — blank lines stripped, books 17/18/19 deleted as invalid —
#   shrinks the claim to nothing while every stage below still prints GREEN. These
#   four numbers ARE the claim.
[ "$(wc -c < "$TABLE" | tr -d ' ')" = 14852 ] || die "reference table must be 14852 bytes (14850 cells + 2 unconsumed)"
[ "$(awk 'NR>1 && NF==3 && $3 ~ /^[abcd]$/' "$BOOKS" | wc -l | tr -d ' ')" = 982 ] \
  || die "real-books fixture must hold exactly 982 observation rows"
[ "$(awk 'NR>1 && NF==3 && $3 ~ /^[abcd]$/ {print int($1/10)}' "$BOOKS" | sort -un | wc -l | tr -d ' ')" = 43 ] \
  || die "real-books fixture must span exactly 43 distinct books"
[ "$(awk 'NR>1 && $1 ~ /^1[789]/ && NF==3' "$BOOKS" | wc -l | tr -d ' ')" = 61 ] \
  || die "real-books fixture must keep the 61 rows for books 17/18/19 — they are REAL books"
grn "   fixtures pinned: 982 rows · 43 books · 61 rows for books 17/18/19 · table 14852 bytes"

echo
echo "== 2. COMPILE (-Werror, C99) =="
$CC -std=c99 -O0 -Wall -Wextra -Werror -o "$BIN" pb-formula.c || die "the shipped header must compile clean under C99 -Werror"
grn "   clean"

echo
echo "== 3. PROPERTIES (no fixture involved) =="
"$BIN" props || die "self-consistency properties"
grn "   588 offsets consumed exactly · all cells A-D · sections tile 1..150 ·"
grn "   section 6 = one question per page over 1000 seeds · all 4 score bands reachable"

echo
echo "== 4. RED CONTROLS — each must FAIL, on the fixture named for it =="

# ⚠ A MUTANT EXITS NON-ZERO BY DESIGN — that is the whole point of a red control.
#   With `set -e -o pipefail` a bare `$(mutant ... | field ...)` therefore ABORTS
#   the script, silently, right where the controls should be printing. Every
#   mutant run is captured with `|| true` and the fields read from the text.
runm() { "$1" "$2" "$3" q 2>&1 || true; }
field_of() { printf '%s\n' "$1" | grep "^$2=" | cut -d= -f2; }

m=$(mutant live_first 's/? first :/? pat[0] :/') || exit 1
ob=$(runm "$m" books "$BOOKS"); ot=$(runm "$m" table "$TABLE")
bw=$(field_of "$ob" BOOKS_WRONG); bl=$(field_of "$ob" BOOKS_WRONG_LIST); tw=$(field_of "$ot" TABLE_WRONG)
[ "$bw" = 22 ] && [ "$bl" = "2,47,92" ] && [ "$tw" = 3681 ] \
  || die "live-first control: got books=$bw [$bl] table=$tw, want 22 [2,47,92] 3681"
red "   R1 first-capture read live      books 22/925  ONLY books 2,47,92   table 3681"
echo "      ⚠ 903 of 925 real observations stay GREEN under a REAL bug. This is the"
echo "        carga n=1 lesson: the control that fires almost nowhere is the one"
echo "        nobody audits, so its three books are asserted BY NAME above."

m=$(mutant shift_q 's/#define PB_SHIFT_Q         14/#define PB_SHIFT_Q         15/') || exit 1
ob=$(runm "$m" books "$BOOKS"); ot=$(runm "$m" table "$TABLE")
bw=$(field_of "$ob" BOOKS_WRONG); tw=$(field_of "$ot" TABLE_WRONG)
[ "$bw" = 370 ] && [ "$tw" = 10175 ] || die "shift_q control: got books=$bw table=$tw, want 370 10175"
red "   R2 shift position 14 -> 15      books 370/925                      table 10175"

m=$(mutant drop_wrap 's/if (q % PB_PER_ROUND == PB_SHIFT_Q || q == PB_QUESTIONS - 1) {/if (q % PB_PER_ROUND == PB_SHIFT_Q) {/') || exit 1
"$m" props >/dev/null 2>&1 && die "drop-wrap control must fail the offset-budget property"
ot=$(runm "$m" table "$TABLE"); tw=$(field_of "$ot" TABLE_WRONG)
[ "$tw" = 9749 ] || die "drop_wrap control: got table=$tw, want 9749"
red "   R3 q==149 pop deleted           consumes 490 not 588 (props RED)   table 9749"

m=$(mutant prev_self 's/: pat\[q + 1\];/: pat[q];/') || exit 1
ob=$(runm "$m" books "$BOOKS"); ot=$(runm "$m" table "$TABLE")
bw=$(field_of "$ob" BOOKS_WRONG); tw=$(field_of "$ot" TABLE_WRONG)
[ "$bw" = 557 ] && [ "$tw" = 11038 ] || die "prev_self control: got books=$bw table=$tw, want 557 11038"
red "   R4 chain reads self not next    books 557/925                      table 11038"

m=$(mutant offset_582 's/"013020120331"/"013020220331"/') || exit 1
ob=$(runm "$m" books "$BOOKS"); ot=$(runm "$m" table "$TABLE")
bw=$(field_of "$ob" BOOKS_WRONG); tw=$(field_of "$ot" TABLE_WRONG)
[ "$tw" = 1 ] || die "offset_582 control: got table=$tw, want exactly 1"
[ "$bw" = 0 ] || die "offset_582 control: got books=$bw, want exactly 0 (it must be BLIND)"
red "   R5 offset digit 582 bumped      books 0/925  EXPECTED BLIND        table 1"
echo "      ⚠ ANNOUNCED, not hidden. Offset digits 546-587 are provably invisible to"
echo "        every real-book observation that exists, and this one moves exactly ONE"
echo "        cell of 14850. A sampled port check would miss it entirely - which is"
echo "        precisely why fixture (b) exists and why stage 6 is a full compare."
echo
echo "   ⚠ WHERE THE CONTROLS DO *NOT* GO RED, stated so it survives a context reset:"
echo "     . deleting the ENTIRE transform still leaves ~40% of the real rows green,"
echo "       led by all 150 book-1 observations - book 1 SKIPS the transform."
echo "     . mutating the seed goes red almost everywhere, so it is USELESS as a"
echo "       control: it proves a constant loaded, which was never in doubt."

echo
echo "== 5. GREEN — the FORMULA is right, vs REAL PRINTED BOOKS =="
"$BIN" books "$BOOKS" || die "the formula disagrees with a real observed book answer"
grn "   0 wrong"

echo
echo "== 6. GREEN — the PORT is right, vs the REFERENCE GENERATOR =="
"$BIN" table "$TABLE" || die "the port diverges from the reference generator"
grn "   0 wrong across all 14850 cells (full compare, not a sample)"

echo
echo "== 7. VACUITY BUDGET — what this suite CANNOT see =="
"$BIN" budget "$BOOKS"

echo
grn "ALL SEVEN STAGES PASSED."
echo "Honest headline — say it this way, never 'verified against the toy':"
echo "  the port reproduces the reference generator exactly (14850/14850, CIRCULAR),"
echo "  and the generator matches 925 distinct real-book observations across 43"
echo "  books, of which 150 test only the seed constant."

#!/usr/bin/env bash
# run.sh — host proof harness for devices/esp32-pense-bem/pbgame.h
#
#   bash devices/esp32-pense-bem/game-test/run.sh
#
# The session rules — scoring, retries, advance, banding — are the parts a player
# could never notice being wrong. This suite is why they are a pure header and
# not logic buried in the sketch. Written test-first: pb-game.c was RED before
# pbgame.h existed.
set -euo pipefail
cd "$(dirname "$0")"

CC=${CC:-cc}
HDR=../pbgame.h

red() { printf '\033[31m%s\033[0m\n' "$*"; }
grn() { printf '\033[32m%s\033[0m\n' "$*"; }
die() { printf '\033[31mFAIL: %s\033[0m\n' "$*" >&2; exit 1; }

work=$(mktemp -d); trap 'rm -rf "$work"' EXIT

# ⚠ A MUTATION THAT DOES NOT APPLY IS NOT A RED-ON-DEMAND PROOF, and die() must
#   write to STDERR because this runs inside $( ) — on stdout the message would
#   be captured into the variable and the guard would fail silently.
mutant() {
  local name="$1"
  local expr="$2"
  local dir="$work/$name"
  mkdir -p "$dir/game-test"
  cp ../pensebem.h "$dir/pensebem.h"
  sed "$expr" "$HDR" > "$dir/pbgame.h"
  cmp -s "$HDR" "$dir/pbgame.h" && die "mutation '$name' DID NOT APPLY — its sed pattern no longer matches"
  cp pb-game.c "$dir/game-test/pb-game.c"
  ( cd "$dir/game-test" && $CC -std=c99 -O0 -w -o pb-game pb-game.c ) \
    || die "mutant '$name' did not compile — that is not a proof either"
  echo "$dir/game-test/pb-game"
}

echo "== 1. PURITY =="
code_only() { sed -e 's,//.*,,' -e '/^[[:space:]]*\*/d' -e '/^[[:space:]]*\/\*/d' "$1"; }
for banned in '<Arduino.h>' 'String ' 'Serial\.' 'millis(' 'pinMode' 'digitalWrite' 'malloc(' 'float ' 'double '; do
  code_only "$HDR" | grep -q -- "$banned" && die "pbgame.h must stay Arduino-free; found: $banned"
done
grn "   pbgame.h is dependency-free"

echo
echo "== 2. COMPILE (-Werror, C99) =="
$CC -std=c99 -O0 -Wall -Wextra -Werror -o ./pb-game pb-game.c || die "pbgame.h must compile clean"
grn "   clean"

echo
echo "== 3. RED CONTROLS — each mutation must make the suite FAIL =="
check_red() {
  local label="$1" name="$2" expr="$3"
  local m; m=$(mutant "$name" "$expr") || exit 1
  "$m" >/dev/null 2>&1 && die "$label: the suite stayed GREEN under a real rule change"
  red "   $label"
}
check_red "R1 every try pays 10 (attempt ignored)" pts \
  's/pb_points_for_attempt(g->attempt)/pb_points_for_attempt(1)/'
check_red "R2 a stray key BURNS a try"             stray \
  "s/if (c < 'A' || c > 'D') {/if (0) {/"
check_red "R3 first wrong answer reveals at once"  tries \
  's/if (g->attempt < PB_MAX_ATTEMPTS) {/if (0) {/'
check_red "R4 band taken from the RAW score"       band \
  's/return pb_band(pb_game_normalized(g));/return pb_band(g->raw);/'
check_red "R5 four tries allowed instead of three" maxatt \
  's/#define PB_MAX_ATTEMPTS 3/#define PB_MAX_ATTEMPTS 4/'
echo "      ⚠ R4 is the one that hides best: with 30 questions a raw score never"
echo "        exceeds 300 but a BAND boundary sits at 76, so two of the four"
echo "        end-of-session songs would simply never play and nothing would error."

echo
echo "== 4. GREEN =="
./pb-game || die "the session rules"
grn "   scoring 10/6/4 · retry stays on the question · 3rd wrong reveals and advances"
grn "   a stray key never burns a try · all 4 bands reachable · review round ascends by page"
echo
grn "ALL STAGES PASSED."

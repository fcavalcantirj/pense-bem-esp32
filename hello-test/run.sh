#!/usr/bin/env bash
# run.sh — host proof harness for pbhello.h, the "say hello once" payload.
#
#   bash hello-test/run.sh
#
# This is the only part of the firmware that sends anything anywhere, so it is
# the part that gets the most suspicious treatment. Written against a server
# that validates strictly: a board whose UUID is rejected works perfectly and is
# simply never counted, which is invisible from both ends.
set -euo pipefail
cd "$(dirname "$0")"

CC=${CC:-cc}
HDR=../pbhello.h
INO=../pense-bem-esp32.ino

red() { printf '\033[31m%s\033[0m\n' "$*"; }
grn() { printf '\033[32m%s\033[0m\n' "$*"; }
die() { printf '\033[31mFAIL: %s\033[0m\n' "$*" >&2; exit 1; }

work=$(mktemp -d); trap 'rm -rf "$work"' EXIT

# ⚠ A MUTATION THAT DOES NOT APPLY IS NOT A RED-ON-DEMAND PROOF, and die() must
#   write to STDERR because this runs inside $( ) — on stdout the message would
#   be captured into the variable and the guard would fail silently.
mutant() {
  local name="$1" expr="$2"
  local dir="$work/$name"
  mkdir -p "$dir/hello-test"
  sed "$expr" "$HDR" > "$dir/pbhello.h"
  cmp -s "$HDR" "$dir/pbhello.h" && die "mutation '$name' DID NOT APPLY — its sed pattern no longer matches"
  cp pb-hello.c "$dir/hello-test/pb-hello.c"
  ( cd "$dir/hello-test" && $CC -std=c99 -O0 -w -o pb-hello pb-hello.c ) \
    || die "mutant '$name' did not compile — that is not a proof either"
  echo "$dir/hello-test/pb-hello"
}

echo "== 1. PURITY =="
code_only() { sed -e 's,//.*,,' -e '/^[[:space:]]*\*/d' -e '/^[[:space:]]*\/\*/d' "$1"; }
for banned in '<Arduino.h>' 'String ' 'Serial\.' 'millis(' 'WiFi' 'HTTPClient' 'malloc(' 'float ' 'double '; do
  code_only "$HDR" | grep -q -- "$banned" && die "pbhello.h must stay Arduino-free; found: $banned"
  # ⚠ pbhmac.h is held to the same rule for the same reason: the host suite has
  #   to compile the SAME file the board runs. A mirror is not a test.
  code_only ../pbhmac.h | grep -q -- "$banned" && die "pbhmac.h must stay Arduino-free; found: $banned"
done
# ⚠ `unsigned long` IS BANNED OUTRIGHT IN pbhmac.h, and this is a scar, not
#   taste: it is 64-bit on the Mac and 32-bit on the ESP32. SHA-256 is defined on
#   exact 32-bit words, so that one type would make the host suite green while
#   the board computed a different digest — which is precisely the bug this
#   project already shipped once, in the game formula.
code_only ../pbhmac.h | grep -qE 'unsigned long|unsigned int |[^_a-z]long ' \
  && die "pbhmac.h uses a width-varying integer type; use uint32_t/uint64_t"
grn "   pbhello.h and pbhmac.h are dependency-free, fixed-width only"

echo
echo "== 2. COMPILE (-Werror, C99) =="
$CC -std=c99 -O0 -Wall -Wextra -Werror -o ./pb-hello pb-hello.c || die "pbhello.h must compile clean"
grn "   clean"

echo
echo "== 3. THE MAC ADDRESS IS NOWHERE IN THE FIRMWARE =="
# ⚠ A privacy property, enforced by grep because no unit test can see it. The
#   payload is a random UUID precisely so a hardware identifier never leaves the
#   board; the cheapest way that promise breaks is someone reaching for
#   WiFi.macAddress() because it is one line shorter than NVS.
if code_only "$INO" | grep -qE 'macAddress|esp_efuse_mac|esp_read_mac'; then
  die "the sketch references a MAC address — the payload must never carry one"
fi
grn "   no MAC accessor anywhere in the sketch"

echo
echo "== 4. THE FEATURE IS ACTUALLY WIRED IN =="
# ⚠ THIS STAGE EXISTS BECAUSE IT ALREADY HAPPENED. helloTick() was written,
#   reviewed, compiled, linked and flashed to a real board — and never called.
#   Every suite here was GREEN, build.sh exited 0, and `strings` on the image
#   found the hostname and both disclosure pages, because they were all
#   genuinely compiled in. The board simply never phoned home, and the only
#   thing that noticed was an empty file on the server.
#
#   A unit test proves a function is correct. Nothing proves it is REACHED.
#   That gap is invisible to every kind of check except this one.
grep -q 'helloTick();' <(sed -n '/^void loop()/,/^}/p' "$INO") \
  || die "helloTick() is not called from loop() — the feature is dead code"
grn "   helloTick() is called from loop()"

grep -q 'helloDisclose();' "$INO" \
  || die "helloDisclose() is never called — boards would send without disclosing"
grn "   helloDisclose() is called before anything is sent"

# The disclosure must be gated on the CONSENT check, not shown unconditionally
# and not skipped unconditionally.
grep -q 'pb_hello_needs_disclosure(' "$INO" \
  || die "the disclosure is not gated on pb_hello_needs_disclosure()"
grn "   the disclosure is gated on the stored consent version"

# Same class again: a network selector nothing calls means a board that only
# ever tries whichever AP setup() happened to name last.
grep -q 'wifiTick();' <(sed -n '/^void loop()/,/^}/p' "$INO") \
  || die "wifiTick() is not called from loop() — the hotspot fallback is dead code"
grn "   wifiTick() is called from loop()"

# ⚠ WiFiMulti.run() blocks for seconds while it scans. This sketch's first rule
#   is that WiFi never blocks the game, and that rule is not enforceable by any
#   test that runs on the host — only by refusing the API outright.
if code_only "$INO" | grep -q 'WiFiMulti'; then
  die "WiFiMulti blocks while scanning; the non-blocking wifiTick() alternation exists instead"
fi
grn "   no blocking WiFi API in the sketch"

# ⚠ A DEBUG LINE THAT SHIPS IS WORSE THAN A BUG, because it works.
#   `prefs.remove("hello_ok")` was added for ten minutes to force the consent
#   screen in front of a camera. Left in, it would re-show the disclosure on
#   EVERY boot of EVERY board forever — no crash, no error, just a device that
#   nags. It survived a build, a flash and a commit-staging before being caught
#   by eye. Greps are cheap; this one is not negotiable.
if grep -nE 'TEMP:|XXX:|FIXME|prefs\.remove\("hello_ok"\)|PB_DEBUG' "$INO"; then
  die "a temporary/debug line is still in the sketch (see the match above)"
fi
grn "   no temporary or debug lines in the sketch"

# ⚠ SAME CLASS AS helloTick(): the signature can be computed perfectly and never
#   attached, or attached and never computed. Either way every board reads
#   unsigned, nothing errors, and the only symptom is a number on a server.
grep -q 'pb_hmac_hex(' "$INO" \
  || die "the payload is never signed — pb_hmac_hex() is not called in the sketch"
grn "   the payload is signed before it is sent"

grep -q 'addHeader("X-PB-Sign"' "$INO" \
  || die "the signature is computed but never sent — no X-PB-Sign header is added"
grn "   the signature is attached to the request"

# ⚠ HYPHENS, NOT UNDERSCORES. nginx drops headers containing underscores by
#   default, so X_PB_Sign would vanish in transit with nothing reporting it.
grep -q 'X_PB_Sign' "$INO" \
  && die "the header name uses underscores; nginx silently drops those"
grn "   the header name cannot be dropped by nginx"

# The key must come from secrets.h, never be typed into a tracked file.
grep -qE '#define[[:space:]]+PB_HELLO_KEY' "$INO" \
  && die "PB_HELLO_KEY is defined in the sketch — it belongs in gitignored secrets.h"
grn "   the key is not hard-coded in a tracked file"

echo
echo "== 4b. THE DISCLOSURE FITS THE PANEL =="
# ⚠ centreFit has no font below 9pt, so an over-wide body line CLIPS rather
#   than shrinking. Measured against the real TFT_eSPI tables, reading the same
#   strings pbhello.h gives the sketch. Three lines failed this the first time
#   it ran — including "how to turn it off", in both languages.
GFXFF=${GFXFF:-$HOME/Documents/Arduino/libraries/TFT_eSPI/Fonts/GFXFF}
if [ -d "$GFXFF" ]; then
  rm -f ./fit
  $CC -std=c99 -O0 -w -I "$GFXFF" -o ./fit fit.c || die "fit.c did not compile"
  [ -x ./fit ] || die "fit binary missing — refusing to read a stale result"
  ./fit || die "the disclosure does not fit the panel"
  grn "   every disclosure line fits with margin"
else
  echo "   ⚠ SKIPPED: TFT_eSPI fonts not found at $GFXFF"
  echo "     This is a GAP, not a pass — the disclosure width is unverified here."
fi

echo
echo "== 4c. THE SIGNATURE MATCHES PUBLISHED VECTORS =="
# ⚠ Checked against RFC 4231 and NIST constants, NOT against itself. The Go
#   server is checked against the same numbers, so the two ends agreeing is
#   evidence; two implementations by the same author agreeing would not be.
$CC -std=c99 -O0 -Wall -Wextra -Werror -o ./pb-hmac-test pb-hmac.c || die "pb-hmac.c did not compile"
./pb-hmac-test || die "HMAC-SHA256 does not match the published test vectors"
grn "   every SHA-256 and HMAC vector matched"

echo
echo "== 5. RED CONTROLS — each mutation must make the suite FAIL =="
check_red() {
  local label="$1" name="$2" expr="$3"
  local m; m=$(mutant "$name" "$expr") || exit 1
  "$m" >/dev/null 2>&1 && die "$label: the suite stayed GREEN under a real change"
  red "   $label"
}
check_red "R1 uuid version nibble not forced"  ver \
  's/b\[6\] = (unsigned char)((b\[6\] \& 0x0F) | 0x40);/b[6] = b[6];/'
check_red "R2 uuid variant nibble not forced"  var \
  's/b\[8\] = (unsigned char)((b\[8\] \& 0x3F) | 0x80);/b[8] = b[8];/'
check_red "R3 build stamp no longer sanitised" san \
  's/int ok = (c >= .a. \&\& c <= .z.)/int ok = 1 || (c >= 0x61 \&\& c <= 0x7a)/'
check_red "R4 oversized payload truncates instead of refusing" cap \
  's/if (need + 1 > outCap) return 0;/if (0) return 0;/'
check_red "R5 consent never re-asked after a payload change" con \
  's/return storedConsentVersion < PB_HELLO_PAYLOAD_VERSION;/return 0;/'

# ── the signature. Mutations go into pbhmac.h and pb-hmac.c must notice.
hmac_red() {
  local label="$1" name="$2" expr="$3"
  local dir="$work/$name"
  mkdir -p "$dir/hello-test"
  sed "$expr" ../pbhmac.h > "$dir/pbhmac.h"
  cmp -s ../pbhmac.h "$dir/pbhmac.h" && die "mutation \'$name\' DID NOT APPLY"
  cp pb-hmac.c "$dir/hello-test/pb-hmac.c"
  ( cd "$dir/hello-test" && $CC -std=c99 -O0 -w -o t pb-hmac.c ) \
    || die "mutant \'$name\' did not compile — that is not a proof either"
  "$dir/hello-test/t" >/dev/null 2>&1 && die "$label: the vectors still matched under a real change"
  red "   $label"
}
hmac_red "R6 HMAC inner pad constant wrong (0x36)" ipad \
  's/\^ 0x36u/^ 0x37u/'
hmac_red "R7 HMAC outer pad constant wrong (0x5c)" opad \
  's/\^ 0x5cu/^ 0x5du/'
hmac_red "R8 over-long key truncated instead of hashed" longkey \
  's/if (keylen > PB_SHA256_BLOCK) {/if (0) {/'
hmac_red "R9 SHA-256 second padding block skipped" pad2 \
  's/if (i > 56) {/if (0) {/'
hmac_red "R10 digest emitted uppercase" case \
  's/"0123456789abcdef"/"0123456789ABCDEF"/'

echo
echo "      ⚠ R8 and R10 are the two that would ship silently. Our key is short,"
echo "        so the over-long branch is never taken in the field; and the server"
echo "        compares the header as TEXT, so uppercase hex is simply rejected."
echo "        Both would leave every board unsigned with nothing reporting a fault."

echo
echo "      ⚠ R5 is the one that hides best. Every other mutation breaks a value"
echo "        someone could see; R5 only removes a QUESTION. A board that stops"
echo "        asking still counts, still plays, still looks perfect — and the"
echo "        promise on its screen quietly stops being true."

echo
echo "== 6. GREEN AGAIN =="
./pb-hello || die "the unmutated header must pass"
grn "   pbhello.h passes clean"

cat <<'BLIND'

── what this suite CANNOT see (printed every run, on purpose)
   · Entropy quality. It proves the FORMAT of a UUID, never that esp_random()
     returned anything unpredictable. A board seeded badly would emit valid,
     colliding UUIDs and under-count — silently.
   · The disclosure SCREEN. Whether the notice actually renders unclipped in
     both languages is a rendering fact, and rendering bugs are invisible to
     every test here. Two have already shipped on this board. Look at it.
   · The network path. Nothing here sends a byte; that the POST never blocks
     gameplay and never retries in a loop is proven on hardware, not in C.
BLIND
echo
grn "ALL STAGES PASSED"

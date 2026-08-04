# AGENTS.md

Machine-readable guide for coding agents working in this repository.
Human version: [INSTALL.md](INSTALL.md) · Project overview: [README.md](README.md)

---

## What this is

ESP32 firmware for a LilyGO T-Display V1.1 that recreates a 1988 activity-book
toy. It stores no answers; it derives all 14 850 from two constants.

**Layers, and the rule that separates them:**

| file | owns | may not |
|---|---|---|
| `pensebem.h` | the answer formula | include Arduino headers, do I/O |
| `pbgame.h` | session rules — scoring, retries, advance, banding | include Arduino headers, do I/O |
| `pense-bem-esp32.ino` | buttons and pixels | own any rule a host test could check |

**The renderer formats; it never computes.** If you can test it on a host, it
does not belong in the `.ino`.

---

## Setup

```bash
brew install arduino-cli
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.10      # PINNED — build.sh enforces
arduino-cli lib install TFT_eSPI@2.5.43

SEL=~/Documents/Arduino/libraries/TFT_eSPI/User_Setup_Select.h
sed -i '' 's|^#include <User_Setup.h>|//#include <User_Setup.h>|' "$SEL"
sed -i '' 's|^//#include <User_Setups/Setup25_TTGO_T_Display.h>|#include <User_Setups/Setup25_TTGO_T_Display.h>|' "$SEL"

cp secrets.h.example secrets.h                   # gitignored
```

## Verify

```bash
bash formula-test/run.sh     # 7 stages — exit 0 required
bash game-test/run.sh        # 4 stages — exit 0 required
./build.sh                   # runs both suites, then compiles
```

⚠ **Check exit codes, not output.** `./build.sh | grep PASSED` reports the
grep's status, not the build's. This mistake was made twice in this repo's
history and reported a broken build as working. Use `./build.sh && echo ok`.

## Flash

```bash
./build.sh flash    # USB. /dev/cu.usbserial*, 115200 (NOT the 921600 default)
./build.sh ota      # WiFi. Requires a prior USB flash.
```

---

## Hard constraints

Violating any of these produces a defect that tests will not catch.

1. **Never copy the seed or offset constants out of `pensebem.h`.**
   `formula-test/run.sh` stage 1 fails if they appear in any other file. That
   grep is the only thing keeping *"the tests read the shipped code"* true.
2. **`GPIO0` (OK, left button) gets short presses only.** It is a strapping pin:
   held LOW at boot it enters USB download mode, and holding it through a
   blocking call can starve the task watchdog. Every hold gesture goes on
   `GPIO35`.
3. **`pinMode(35, INPUT)` — never `INPUT_PULLUP`.** GPIO34–39 are input-only with
   no internal pull-up hardware; `INPUT_PULLUP` compiles, runs, and configures
   nothing. The board supplies an external pull-up.
4. **All composed text goes through `centreFit()` or `drawHints()`.** Both
   measure with `tft.textWidth()` and step down a font until the text fits.
   Direct `drawString` of variable text will clip or collide — both have shipped.
5. **TFT_eSPI fonts 6/7/8 are digit/clock faces, not ASCII.** Letters render as
   garbage. Use FreeFonts, or built-in fonts 1/2/4.
6. **`ledcAttach` *after* `tft.init()`.** `tft.init()` reclaims its backlight pin
   and clobbers a PWM channel attached before it. Also: `ledcSetup` and
   `ledcAttachPin` were **removed in core 3.0** — the API is pin-based now.
7. **Declare types above the first function.** The Arduino build injects
   generated prototypes at the top of the `.ino`, so a `struct` defined halfway
   down errors via a prototype you never wrote.
8. **No `delay()` in the sound player.** It would freeze the button scan and the
   idle timer.
9. **No game path may read the network.** The answer is pure arithmetic and the
   game plays identically with every access point down. WiFi carries exactly two
   things: OTA reflashing, and **one POST per boot** to `api.pense-bem-wars.com`
   containing a random UUID the board generated itself plus `PB_VERSION` —
   nothing else, never the MAC. It is disclosed on screen at first boot in both
   languages and compiled out entirely by `#define PB_PHONE_HOME 0`.
   ⚠ This constraint previously read *"WiFi is for OTA only"*; it was amended on
   2026-08-03 when that stopped being true, rather than left to mislead.
   See `pbhello.h` and `hello-test/`.
10. **Keep the folder name `pense-bem-esp32`.** `arduino-cli` requires the `.ino`
    basename to equal its directory. A clone into any other name will not build.

---

## Testing doctrine

This repo takes evidence seriously. Match it.

**An assertion is vacuous when its two sides share a term.** They co-vary into a
tautology instead of constraining each other.

**Two fixtures, two different claims. Do not conflate them:**

- `real-books.tsv` — answers read off real hardware with real printed books.
  **The only thing that can falsify the formula.** Covers 925 of 14 850 cells.
- `reference-table.txt` — the upstream generator's own output. Proves the **port**
  across all 99×150. ⚠ **Circular** — generated *by* the formula it would vouch
  for. `run.sh` proves this at runtime: the file's first 150 characters are the
  seed constant.

**When adding a check, prove it can fail.** Mutate the source, confirm red,
revert, confirm green. Then ask *where it does not go red* — measured here:
deleting the entire transform still leaves **~40 %** of real-book rows passing,
because book 1 skips the transform and dominates the sample.

**Name degenerate inputs explicitly.** The `first`-capture bug is visible to only
**22 of 925** observations, in books **2, 47 and 92** — so those three are
asserted by name. A control that fires almost nowhere is the one nobody audits.

**A mutation that does not apply is not a proof.** `run.sh` `cmp`s the mutated
copy against the original and fails loudly if the patch stopped matching.

**Host tests cannot see the target.** `unsigned long` is 64-bit on a dev machine
and **32-bit on the ESP32**; that difference silently broke the review-round LCG
while the host suite stayed green. Use fixed-width types for anything that
overflows.

**Rendering bugs are invisible to tests.** Two shipped: a clipped title and a
pair of colliding hints. Both were found by looking at the screen. If you change
drawing code, look at the hardware.

---

## Is a board running this code?

The build stamp is on the standby screen (top right) and on serial at boot:

```
pense-bem a1b2c3d  |  offsets popped=588 (must be 588)
```

Compare with `git describe --always --dirty`. A `-dirty` suffix means
uncommitted changes are in that binary. `version.h` is generated by `build.sh`
and is gitignored — never commit it, never hand-edit it.

---

## Licence

Project code is MIT. **The constants in `pensebem.h` are not** — they are
Beerware Revision 42 reverse-engineering work by Eduardo Habkost, Leandro Pereira
and Felipe Sanches. `NOTICE` must travel with them, verbatim, unparaphrased.

⚠ `formula-test/fixtures/real-books.tsv` comes from an upstream repository that
ships **no licence file**. It is vendored with attribution and a pinned commit.
Do not redistribute it further without resolving that.

# Pense Bem · ESP32

*[Leia em português](LEIAME.md)* · **[pense-bem-wars.com](https://pense-bem-wars.com)**

A working recreation of the **Pense Bem** (Tectoy, Brazil, 1988) — the electronic
activity-book toy — on a LilyGO T-Display V1.1. Two buttons, a colour screen, no
database, no cloud, and no internet needed to play.

You type a three-digit code, open the matching printed book, and answer thirty
multiple-choice questions. The device tells you if you're right.

**It does not store a single answer.** It *derives* all 14 850 of them from two
constants and six lines of arithmetic — which is the whole reason this project is
interesting.

---

### ⚠ One thing it sends · Uma coisa que ele envia

**EN** — Once per boot, the board POSTs to `api.pense-bem-wars.com`: **a random
UUID it generated on itself, the firmware build stamp, and a signature over
those two. Nothing else.** Not your MAC address, not a score, not an answer, not
which book you opened. It says so on screen the first time you switch it on, in
Portuguese and English.

It exists for one reason: I set a gate before publishing — **50 GitHub stars and
5 boards actually flashed** — and I wanted the second number measured instead of
guessed. The count is *boards that said hello*, never *users*.

**About that signature, plainly.** The server publishes two numbers:
`{"boards":N,"signed":M}`. `boards` counts anything well-formed and **is still
spoofable with one `curl`** — that has not changed. `signed` counts requests that
carried a correct HMAC-SHA256 under a key that is not in this repo, so a flood of
forged POSTs moves the first number and cannot move the second. That is the only
thing it buys: a flood becomes *visible* rather than silently mixed in.

⚠ **It is not a lock, and I would rather say so than be caught claiming it.** If
you build this firmware yourself you will not have my key, so your board reports
**unsigned — and it still counts**. Nothing is refused, no feature is lost, the
game is identical. There is no arrangement in which public firmware holds a
secret an attacker cannot also hold; that is information theory, not a gap I
forgot to close. (Nor is there a hardware way out on this chip: the only
device-unique value on a classic ESP32 is the factory eFuse **MAC**, which is
broadcast in every WiFi frame — so it identifies without authenticating, and this
project refuses to send it anyway.)

**Turn it off completely:** set `#define PB_PHONE_HOME 0` in
`pense-bem-esp32.ino`. That removes the request, the UUID, the consent screen and
the entire HTTP stack — 124 KB, and the hostname is verifiably absent from the
built image.

⚠ **Why that switch matters beyond preference:** the hostname is compiled into the
binary. The domain is registered to **2027-08-04** with auto-renew on — but if it
ever lapsed, boards still running this firmware would POST to whoever registered
it next, and nobody can reflash your board but you. That switch is your own
defence, not a setting.

**PT** — Uma vez por inicialização, a placa envia para `api.pense-bem-wars.com`:
**um UUID aleatório que ela mesma gerou, a versão do firmware e uma assinatura
dessas duas coisas. Nada mais.** Não envia o endereço MAC, nem pontuação, nem
resposta, nem qual livro você abriu. Ela avisa na tela na primeira vez que você
liga, em português e em inglês.

Sobre a assinatura, sem enfeite: o servidor publica dois números,
`{"boards":N,"signed":M}`. O primeiro conta qualquer requisição bem formada e
**continua sendo falsificável com um `curl`** — isso não mudou. O segundo exige
uma chave que não está neste repositório, então uma enxurrada de POSTs forjados
mexe no primeiro número e não mexe no segundo. É só isso que ela compra: a
falsificação fica *visível*, não impedida.

⚠ **Não é cadeado.** Se você compilar este firmware, não terá a minha chave: a
sua placa reporta **sem assinatura — e continua sendo contada**. Nada é recusado,
nada é perdido, o jogo é idêntico. Não existe arranjo em que um firmware público
guarde um segredo que o atacante não possa ter também.

Para desligar de vez: `#define PB_PHONE_HOME 0` no `pense-bem-esp32.ino`.

---

**Want the multiplayer version?** ⭐ **[Star this repo](https://github.com/fcavalcantirj/pense-bem-esp32)** —
that star is literally the vote, and the gate above is real. Below it, *Pense Bem
Wars* does not get built.

☕ [Buy me a coffee](https://buymeacoffee.com/fcavalcantirj) — never required, and
there is nothing to buy here. You buy your own ~US$15 board and the code is free.

<p align="center">
  <img src="docs/splash.gif" width="420" alt="Boot: ~900 particles converge into PENSE BEM, then the standby screen">
</p>

<p align="center"><sub>Boot. The title is not drawn — it is <b>assembled</b>: the text is
rendered to an off-screen sprite and its lit pixels become the targets ~900 particles
fly into.</sub></p>

### See it running

- **[Playing it](https://drive.google.com/file/d/1xkHbjNFMxeBf9py2WNtwjVqcgYjH4kDP/preview)** — code entry, a question, an answer
- **[The boot animation](https://drive.google.com/file/d/15JBqlwcrhhBKz7v20fLi9zfxgVIpkNsA/preview)** — particles assembling into the title
- **[An over-the-air fix landing](https://drive.google.com/file/d/1ibkhmrhZjWPmG_aU1f9fXT4cUaFk4v45/preview)** — a rendering bug caught on camera, fixed, and reflashed without touching the board

---

## Get it running

You need a **LilyGO T-Display V1.1** and a **USB-C data cable**. Nothing else.
(⚠ a charge-only cable gives a dark screen *and* no serial port, and looks
exactly like a dead board.)

### If you're doing it yourself

```bash
# toolchain — the core version is PINNED; build.sh refuses anything else
arduino-cli core install esp32:esp32@3.3.10
arduino-cli lib install TFT_eSPI@2.5.43

# tell TFT_eSPI it's a T-Display V1.1 (it picks the panel from ONE global file)
SEL=~/Documents/Arduino/libraries/TFT_eSPI/User_Setup_Select.h
sed -i '' 's|^#include <User_Setup.h>|//#include <User_Setup.h>|' "$SEL"
sed -i '' 's|^//#include <User_Setups/Setup25_TTGO_T_Display.h>|#include <User_Setups/Setup25_TTGO_T_Display.h>|' "$SEL"

# ⚠ keep the folder name — arduino-cli needs the .ino basename to match it
git clone https://github.com/fcavalcantirj/pense-bem-esp32.git
cd pense-bem-esp32

cp secrets.h.example secrets.h    # wifi is for OTA only; the game never uses it
./build.sh                        # runs both test suites, then compiles
./build.sh flash                  # first flash over USB
```

Full walkthrough with every trap: **[INSTALL.md](INSTALL.md)**

### If you'd rather an agent do it

Paste this into [Claude Code](https://claude.com/claude-code) (or any coding
agent) in an empty directory:

```text
Flash this ESP32 firmware to my LilyGO T-Display V1.1, which is connected over USB.

  https://github.com/fcavalcantirj/pense-bem-esp32

Read AGENTS.md in that repo FIRST and follow it — it has the pinned toolchain
versions, ten hard constraints, and the traps that will otherwise cost you an hour.

Four that bite immediately:
  - clone into a folder named exactly "pense-bem-esp32"; arduino-cli requires the
    .ino basename to equal its directory name
  - install esp32:esp32@3.3.10 specifically, not latest
  - TFT_eSPI must be switched to Setup25 (T-Display V1.1) in User_Setup_Select.h
  - check EXIT CODES, not output: `./build.sh | grep PASSED` reports the grep,
    not the build. Use `./build.sh && echo ok`.

Ask me for my wifi SSID/password and an OTA password for secrets.h — do not
invent them, and do not commit that file.

Then run both host test suites, compile, and flash over USB. Tell me the flash
percentage and the build stamp when it's done.
```

Then look at the board. **If anything renders wrong, the tests will not tell you** —
three of this project's bugs were only ever visible on the panel.

---

```
┌──────────────────────────────────────────────┐
│ Q07/30          #067            TENT 1/3     │
├──────────────────────────────────────────────┤
│  ┌────┐  ┌────┐  ┌████┐  ┌────┐              │
│  │ A  │  │ B  │  │ C  │  │ D  │              │
│  └────┘  └────┘  └████┘  └────┘              │
├──────────────────────────────────────────────┤
│ < OK responde                       troca >  │
└──────────────────────────────────────────────┘
```

`#067` is the number you look up in the printed book — not your position in the
round (`Q07/30`). Without it, the review section is unusable.

---

## Why the toy is a good story

The original ran on a **Zilog Z8 with 128 bytes of RAM and 2 KB of ROM**. There
was no room to store questions, and no room to store answers either. So it didn't.

Instead, every printed activity book — dozens of them, across years, including
licensed Sonic, Thor and Turma da Mônica titles — was **written to match answers
the device already computes** from `(book, question)`. The booklet is the data.
The toy is a pure function. The "brand" is cosmetic: one formula validates every
book ever printed, including ones nobody has scanned.

That also means **you can author new books.** Ask the generator for book 57's
answer sequence, then write questions whose correct option lands on each letter.
Kids worked this out in the 1990s.

The formula was reverse-engineered by **Eduardo Habkost**, with
**Leandro Pereira** and **Felipe Sanches** (Garoa Hacker Clube, São Paulo), and
published under Beerware. This project ports it, does not rediscover it, and
keeps their notice attached. See [NOTICE](NOTICE).

## The algorithm, in one paragraph

Book 1's 150 answers are a hard-coded seed. Every later book is derived from the
previous one by a position-dependent chained shift:

```c
pattern[q] = 'a' + ((prev - 'a' + shift) % 4);
```

`prev` is the *next* question's answer (`pattern[q+1]`), wrapping at the end to
the previous book's first answer. `shift` is zero except at six positions per
book — `q % 30 == 14`, plus the last — where it consumes the next digit of a
588-digit base-4 queue. 588 = 98 books × 6, consumed exactly, nothing left over.
That budget is a compile-time assertion.

---

## Hardware

| | | where |
|---|---|---|
| **Board** | LilyGO TTGO **T-Display V1.1** — ESP32, 1.14" 240×135 ST7789V IPS, two buttons, USB-C, JST LiPo connector + charging circuit | [AliExpress — official LilyGO store](https://pt.aliexpress.com/item/33050639690.html) · [lilygo.cc](https://lilygo.cc/en-us/products/t-display) |
| **Passive buzzer** *(optional)* | two wires: `GPIO21` and `GND` | any passive/piezo buzzer |
| **Everything else** | nothing. The board is the whole toy. | |

Around **US$12–22** depending on options. Nothing else is required — no
breadboard, no level shifters, no soldering. The screen, both buttons and the
battery charger are already on it, which is most of why this board beats a bare
ESP32 plus a separate display for a project like this.

**Two options worth getting right when you order:**

- **`Com Versão Shell` / "with shell"** — the board plus a moulded case. Worth it
  if this is going to be handled by a child, which is the whole point of the toy.
- **`4MB` vs `16MB CH9102F`** — either works. The firmware is **~1.19 MB, 60 % of
  the 1.97 MB app partition (`min_spiffs`)**, which is the same on both. 16 MB only buys
  you headroom for a larger partition scheme later.

⚠ **Get the V1.1, not the T-Display-S3.** They look alike and are *not*
interchangeable here — the S3 drives its panel over an 8-bit parallel bus, so it
needs a different TFT_eSPI setup (Setup206 vs **Setup25**), a different FQBN, and
different pins. Searching "lilygo t display" will happily show you both.

`SELECT` is the **right** button, `OK` is the **left** one. Every on-screen hint
is drawn on the *side* of the button it refers to, because a board with two
unlabelled buttons and a hint that says "SELECT" is a coin flip. (This project
shipped that hint pointing at the **wrong** button. A tester held the left one for
a while, nothing happened, and nothing was broken. See *Scars*.)

⚠ **Passive buzzer, not active.** An active buzzer has its own oscillator — you
get one fixed pitch and none of the four end-of-round tunes. Meter it first: open
circuit is a piezo (drive it directly); **16–42 Ω is a magnetic coil, so put
100 Ω in series.**

## Build

**Full step-by-step: [INSTALL.md](INSTALL.md).** Working with an agent?
[AGENTS.md](AGENTS.md).

⚠ **Clone into a directory named `pense-bem-esp32`** (the default). `arduino-cli`
requires the sketch file to be named after its folder — rename one and you must
rename the other.

```bash
arduino-cli core install esp32:esp32@3.3.10
arduino-cli lib install TFT_eSPI@2.5.43     # then enable Setup25 in User_Setup_Select.h

cp secrets.h.example secrets.h              # gitignored; WiFi is OTA-only
./build.sh                                  # host suites + pinned-core check + compile
./build.sh flash                            # first flash over USB
./build.sh ota                              # afterwards, over the air
```

`build.sh` **refuses to compile** unless the ESP32 core is pinned at `3.3.10`,
and runs both host test suites before it builds anything. ~60 % flash, 23 % RAM.

⚠ **WiFi exists for OTA reflashing and nothing else.** No game path reads the
network — the answer is arithmetic, and the toy behaves identically with the
access point down.

---

## Layout

| file | |
|---|---|
| `pensebem.h` | **the formula.** Pure C99, no Arduino headers. |
| `pbgame.h` | **the session rules** — scoring, retries, advance, banding. |
| `pense-bem-esp32.ino` | buttons and pixels. Owns no rules. |
| `INSTALL.md` · `AGENTS.md` | setup, for humans and for coding agents |
| [`docs/HOW-THIS-WAS-BUILT.md`](docs/HOW-THIS-WAS-BUILT.md) | the 37 prompts that built this, and the eleven bugs — six of which no test could see |
| `formula-test/` | 7-stage host proof of the formula and the port |
| `game-test/` | host proof of the rules, written test-first |

The two headers are **dependency-free on purpose**: they are compiled by
`xtensa-esp32-elf-g++` *and* by `cc -std=c99 -Wall -Wextra -Werror`. The firmware
and the tests `#include` **the same bytes**, so there is no mirror to drift.
`formula-test/run.sh` greps that the seed constant appears in `pensebem.h` **and
nowhere else** — the moment anyone pastes it into the sketch, "the tests read the
shipped code" quietly stops being true, and that grep is the only thing that
would notice.

```bash
bash formula-test/run.sh
bash game-test/run.sh
```

---

## What the tests actually prove — and what they don't

This is the part worth reading if you only read one section.

There are **two fixtures, making two different claims**, and collapsing them
would turn the whole suite into theatre.

| fixture | proves | cannot prove |
|---|---|---|
| `real-books.tsv` — answers typed off **real hardware with real printed books** | **the formula is right** | coverage: 925 of 14 850 cells, and **56 books have no observations at all** |
| `reference-table.txt` — the upstream generator's own output | **the port is right**, all 99×150 | ⚠ **the formula. It is circular** — it was produced *by* the formula it would be vouching for |

The circularity isn't an opinion. `run.sh` proves it on the spot: the reference
table's **first 150 characters are literally the seed constant.**

So the honest headline — and the project says it this way deliberately, never
*"verified against the toy"*:

> The port reproduces the reference generator exactly (**14 850 / 14 850,
> circular**), and the generator matches **925 distinct real-book observations
> across 43 books**, of which **150 test only the seed constant.**

### The suite prints its own blind spots

Stage 7 does this on every run, because a suite that hides what it can't see
reads as complete six months later:

```
925 distinct real-book observations, 43 books witnessed, 56 books UNWITNESSED
150 of them (16%) are book 1 — which SKIPS the transform entirely and
therefore proves only that a 150-char constant was typed correctly.
shift is zero at 144 of 150 positions: the modular core runs 6 times per book.
measured: deleting the WHOLE transform still leaves ~40% of these rows GREEN.
measured: ~half of the 588 offset digits are invisible to real-book evidence.
```

Read that fourth line again. **You can delete the entire algorithm and 40 % of
the real-world evidence still passes**, because book 1 skips the transform by
definition and dominates the sample.

### Red controls, and where they *don't* fire

Five mutations must make the suite fail, each asserted against the fixture that
can actually see it:

| mutation | red on |
|---|---|
| the `first` capture read live instead of before the loop | **22 / 925 rows — books 2, 47 and 92 ONLY** |
| shift position 14 → 15 | 370 / 925 |
| the wrap-around offset pop deleted | consumes 490 not 588 |
| the chain reads itself instead of the next question | 557 / 925 |
| one offset digit bumped | **exactly 1 of 14 850 — and 0 real rows** |

The first and last rows are the interesting ones.

**A real bug, visible to 2.4 % of the evidence.** Capturing `first` inside the
loop instead of before it corrupts 3681 cells — but none below question 53, so
only three books ever notice. Those three are asserted **by name**, because a
control that fires almost nowhere is the one nobody audits.

**A mutation that is deliberately invisible, and says so.** Offset digits 546–587
cannot be seen by any real-book observation that exists. The suite prints
`EXPECTED BLIND` rather than quietly passing — that's precisely why the second
fixture exists and why the port check is a full 14 850-byte compare, not a sample.

Two controls are *deliberately absent*: mutating the seed constant goes red almost
everywhere, which makes it useless — it proves a constant loaded, which was never
in doubt.

---

## Scars

Things that cost a real debugging session. Kept because they are the most useful
thing here for anyone building on this board.

**`unsigned long` is 64-bit on your laptop and 32-bit on the ESP32.** The
review-round LCG multiplier doesn't fit in 32 bits and `>> 33` is undefined there.
The host suite would have passed green while the device generated a *different*
round. A host harness that can't see the target's word size is a green that means
nothing. It's `unsigned long long` now.

**The camera caught what eleven green stages could not.** First flash rendered the
title clipped off both edges — 24 pt is wider than 240 px. Every test passed and
the screen was unreadable. The fix isn't a smaller font: `centreFit()` measures
with `tft.textWidth()` and steps 24 → 18 → 12 → 9 pt until it fits, so a long
string *shrinks* instead of losing its first and last letters. **Rendering bugs
are invisible to tests and obvious on glass — go look at the thing.**

Three of this project's bugs were found that way, through
**[claude-code-eyes](https://github.com/fcavalcantirj/claude-code-eyes)** — a camera skill that lets a coding agent actually
see the panel it is drawing to.

**The hint pointed at the wrong button.** `SELECT` was drawn left-aligned; SELECT
is the right button. A tester held the left one and nothing happened — which was
*correct behaviour* (`GPIO0` is a strapping pin and deliberately has no hold
gesture) reached through a label pointing the wrong way. Hints are positional now.

**`pinMode(35, INPUT_PULLUP)` compiles, runs, and configures nothing.** GPIO34–39
are input-only with no internal pull-up hardware. The board supplies an external
one.

**Declare types above the first function.** The Arduino build auto-generates
prototypes at the top of every `.ino`, so a `struct` defined halfway down errors
out via a prototype you never wrote.

**`ledcAttach` after `tft.init()`.** `tft.init()` does `pinMode`+`digitalWrite` on
its backlight pin and clobbers a PWM channel attached before it. Also: `ledcSetup`
and `ledcAttachPin` were **removed** in ESP32 core 3.0, and nearly every buzzer
tutorial online still uses them.

---

## Books

Scans of the original activity books are preserved by
[Datassette](https://datassette.org/livros/pense-bem) — free to download and
print at home. The device code is `BBS`: two digits of book, one of section
(1–5 are fixed 30-question blocks; **6 is a review round, one question from each
five-question page**, confirmed empirically against real captures).

⚠ **Books 17, 18 and 19 are valid**, despite an upstream note that reads as
rejecting them. Under that same note's own `BBS` diagram, `017`/`018`/`019` are
book 01, sections 7/8/9 — invalid *sections*. The harvested data settles it: code
`171` carries 30 real observed answers, `181` carries 30, `191` carries 1, and
`017`/`018`/`019` carry none. Only book 00 and section 0 are refused.

---

## Credits, licence, and one open question

The reverse engineering is **not** this project's work. It belongs to Eduardo
Habkost, Leandro Pereira and Felipe Sanches, published under
**Beerware Revision 42** — see [NOTICE](NOTICE), retained verbatim, because
"this stuff" is precisely the two constants in `pensebem.h`.

Upstream: [lpereira/Pense-Bem](https://github.com/lpereira/Pense-Bem) ·
[ehabkost/pensebem](https://github.com/ehabkost/pensebem) ·
[the browser simulator](http://labs.hardinfo.org/pb/)

This project's own code (firmware, test harnesses, game rules) is **MIT** — see
[LICENSE](LICENSE).

⚠ **One honest open item.** `formula-test/fixtures/real-books.tsv` is vendored
from `ehabkost/pensebem`, which **ships no licence file**. It is included here as
a research artifact with full attribution and a pinned upstream commit
(`f6c8b4cc8f240d03da7176aec4cefd62b7ee44f8`). **If the author would rather it not
be redistributed, open an issue and it will be removed** and replaced with a
fetch-on-demand step.

*"Pense Bem"* is a Tectoy trademark. This is an unaffiliated, non-commercial
hobby project. The activity books remain under Tectoy / Nova Cultural copyright —
Datassette preserves them for personal use; don't redistribute the scans.

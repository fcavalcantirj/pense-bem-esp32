# Install

From nothing to a working toy. ~20 minutes, most of it the toolchain download.

There is a machine-readable version of this in [AGENTS.md](AGENTS.md).

---

## 0. What you need

- A **LilyGO T-Display V1.1** ([where to buy](README.md#hardware)) — ⚠ *not* the
  T-Display-S3, which needs a different display driver and FQBN.
- A **USB-C data cable.** ⚠ A charge-only cable gives you a dark screen **and**
  no serial port, and looks exactly like a dead board. This wastes more time
  than any other mistake here — if nothing appears, swap the cable first.
- macOS or Linux. Windows works but the port paths differ.
- *(Optional)* a **passive** buzzer for sound.

---

## 1. Toolchain

```bash
brew install arduino-cli          # or: https://arduino.github.io/arduino-cli/

arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.10
```

⚠ **Pin the core version.** A bare `arduino-cli core install esp32:esp32` drifts
to whatever is newest, and a toolchain that moves under you turns a working
board into a mystery. `build.sh` refuses to compile against anything but
`3.3.10`.

---

## 2. Display library

```bash
arduino-cli lib install TFT_eSPI@2.5.43
```

TFT_eSPI picks its panel from **one global file** in the library folder, so you
must tell it you have a T-Display V1.1:

```bash
SEL=~/Documents/Arduino/libraries/TFT_eSPI/User_Setup_Select.h
sed -i '' 's|^#include <User_Setup.h>|//#include <User_Setup.h>|' "$SEL"
sed -i '' 's|^//#include <User_Setups/Setup25_TTGO_T_Display.h>|#include <User_Setups/Setup25_TTGO_T_Display.h>|' "$SEL"

grep -nE '^#include' "$SEL"     # must show ONLY Setup25
```

*(On Linux, `sed -i` without the `''`.)*

⚠ **This is global state.** If you also build for a T-Display-S3 (which needs
`Setup206`), the two will fight and one will render garbage. Give one of them an
isolated sketchbook via `ARDUINO_DIRECTORIES_USER`. And never run a blind
`arduino-cli lib upgrade` — it resets this file to the default.

---

## 3. Get the code

```bash
git clone https://github.com/fcavalcantirj/pense-bem-esp32.git
cd pense-bem-esp32
```

⚠ **Keep the folder name.** `arduino-cli` requires the `.ino` basename to equal
its directory name. Clone into `pb/` and it will refuse to build.

---

## 4. Configure

```bash
cp secrets.h.example secrets.h
$EDITOR secrets.h
```

Three values. **WiFi is only ever used for over-the-air reflashing** — no game
path touches the network, and the toy works identically with the WiFi wrong or
the router off.

```c
#define WIFI_SSID  "your-ssid"
#define WIFI_PASS  "your-password"
#define OTA_PASS   "pick-something-unique"   // this board's own
```

`secrets.h` is gitignored. Don't commit it.

---

## 5. Build and flash

```bash
./build.sh          # runs both host test suites, then compiles
./build.sh flash    # ⚠ do the FIRST flash over USB
```

`build.sh flash` looks for `/dev/cu.usbserial*` and uploads at **115200** —
deliberately not the 921600 default, which chokes mid-write on this board.

You should see roughly:

```
ALL SEVEN STAGES PASSED.      <- formula proven
ALL STAGES PASSED.            <- game rules proven
build stamp: a1b2c3d
Sketch uses 1047974 bytes (79%) of program storage space.
```

After the first flash, updates go over the air:

```bash
./build.sh ota
```

⚠ **OTA is a one-way door.** If you flash a build whose WiFi credentials are
wrong, or that drops `ArduinoOTA`, the board can only be recovered with a cable.

---

## 6. Play

**`SELECT` is the RIGHT button. `OK` is the LEFT one.** Every on-screen hint is
drawn on the side of the button it means, so you never have to remember this.

1. **OK** to start.
2. Enter a three-digit code — **SELECT** changes the digit (**hold it** to spin),
   **OK** confirms. `⌫` at the end of the cycle erases.
   Two digits of book (`01`–`99`), one of section (`1`–`6`).
3. Open the matching printed book. The screen shows `#067` — that's the
   **question number to look up**, which is not the same as your position in the
   round (`Q07/30`).
4. **SELECT** moves across `A B C D`, **OK** answers. Three tries, then it
   reveals.

Books are preserved at [Datassette](https://datassette.org/livros/pense-bem).
Try code `011` — book 01, section 1. Question 1's answer is **D**.

---

## 7. Sound (optional)

Two wires, and they unplug:

| wire | from | to |
|---|---|---|
| orange | **GPIO21** | passive buzzer **+** |
| black | **GND** | passive buzzer **−** |

⚠ **Meter it first.** Open circuit = piezo, drive it directly. **16–42 Ω is a
magnetic coil — put 100 Ω in series.** And use the **passive** one; an active
buzzer has its own oscillator and can't play the four end-of-round tunes.

Tap reset: **three rising notes** means the wiring works. Silent? Check the
standby screen — if it says `SOM DESLIGADO`, hold the right button to unmute.

Don't want sound? `#define PB_BUZZER_PIN -1` compiles every sound call out.

---

## Is my board running the code in this repo?

Yes, if the build stamp matches. It's in the **top-right of the standby screen**
and printed over serial at boot:

```bash
arduino-cli monitor -p /dev/cu.usbserial* -c baudrate=115200
# pense-bem a1b2c3d  |  offsets popped=588 (must be 588)
```

Compare to `git describe --always --dirty`. A **`-dirty`** suffix means
uncommitted changes went into that binary.

---

## Troubleshooting

| symptom | cause |
|---|---|
| dark screen, no `/dev/cu.usbserial*` | **the cable.** Charge-only USB-C. Swap it before suspecting anything else. |
| `main file missing from sketch` | folder renamed. It must match the `.ino`. |
| garbled / shifted display | TFT_eSPI is not on `Setup25`. Re-check step 2. |
| `'ledcSetup' was not declared` | you are on ESP32 core 2.x. This needs **3.x** — the API is pin-based now. |
| upload dies mid-write | drop to `115200`. `build.sh flash` already does. |
| `could not open port …local` | OTA needs the resolved **IP** plus `-l network`. `build.sh ota` handles it. |
| board unreachable after an OTA | wrong WiFi in `secrets.h`. Recover over USB. |
| held a button, nothing happened | you held **OK** (left). It has no hold gesture on purpose — `GPIO0` is a strapping pin. Hold **SELECT** (right). |

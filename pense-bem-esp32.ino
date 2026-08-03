/* pense-bem-esp32 — the 1988 Tectoy "Pense Bem", book mode, on a LilyGO T-Display V1.1.
 *
 *   SELECT = the RIGHT button (GPIO35)  short = next value · HOLD = auto-repeat
 *   OK     = the LEFT  button (GPIO0)   short = confirm. ⚠ NO HOLD GESTURE, EVER.
 *
 * ⚠ Every on-screen hint is drawn on the SIDE of the button it refers to, because
 *   the board has two unlabelled buttons and a logical name is a 50/50 guess.
 *   (Learned the hard way — see README, "the hint pointed at the wrong button".)
 *
 * THIS SKETCH OWNS NO RULES. It maps two buttons and draws pixels.
 *   pensebem.h  the answer formula   (host-proven: formula-test/)
 *   pbgame.h    the session rules    (host-proven: game-test/, written test-first)
 * Anything a host test could check does not belong in this file.
 *
 * ⚠ NEVER paste the seed or offset constants in here. formula-test/run.sh stage 1
 *   fails if they appear anywhere except pensebem.h — that grep is the only thing
 *   keeping "the tests read the shipped code" true.
 *
 * ⚠ WIFI EXISTS FOR EXACTLY ONE REASON: OTA REFLASHING. No game path reads it.
 *   The answer is pure arithmetic; the game runs identically with the AP down.
 *   If a change ever makes gameplay depend on WiFi, that is a different device.
 *
 * SCARS — every one of these cost a debugging session:
 *   ⚠ GPIO0 IS A STRAPPING PIN. Held LOW at boot it enters USB download mode, and
 *     holding it during a blocking call can starve the task watchdog. It gets
 *     short presses only; every hold gesture lives on GPIO35.
 *   ⚠ GPIO35 IS INPUT-ONLY WITH NO INTERNAL PULL-UP. pinMode(35, INPUT_PULLUP)
 *     compiles, runs, and configures nothing. The board has an external one.
 *   ⚠ TFT_eSPI FONTS 6/7/8 ARE DIGIT/CLOCK FACES, NOT ASCII. Composed text uses
 *     FreeFonts, and every centred string goes through centreFit() so a long one
 *     shrinks instead of losing its first and last letters.
 *   ⚠ ledcAttach AFTER tft.init(). tft.init() does pinMode+digitalWrite on its
 *     backlight pin and clobbers a PWM mux attached before it.
 *   ⚠ Declare types ABOVE the first function. The Arduino build auto-generates
 *     prototypes at the top of every .ino, so a struct defined halfway down
 *     yields an error about a prototype you never wrote.
 *
 * Beerware Rev 42 applies to the constants in pensebem.h — see NOTICE.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <TFT_eSPI.h>
#include <Preferences.h>

#include "secrets.h"
#include "pensebem.h"
#include "pbgame.h"

/* ⚠ TYPES MUST BE DECLARED BEFORE ANY FUNCTION THAT USES THEM, up here next to
   the includes. The Arduino build auto-generates prototypes for every function
   in a .ino and inserts them at the TOP of the file — so a struct defined
   halfway down yields "'Note' does not name a type" from a prototype the author
   never wrote and cannot see. */
struct Note { uint16_t hz; uint16_t ms; };   /* hz == 0 is a rest */

// ---- constants -------------------------------------------------------------

/* ⚠ The mDNS/OTA name deliberately does NOT track the sketch filename. Renaming
   this string renames the board on the network, which breaks `./build.sh ota`
   until the board is reflashed over USB — a chicken-and-egg you do not want. */
static const char *HOSTNAME = "esp32-pense-bem";

// ⚠ PHYSICAL SIDES, AND THE UI DEPENDS ON THEM. With setRotation(1) the sibling
//   sketch's own comment is authoritative: GPIO0 is the LEFT button, GPIO35 is
//   the RIGHT one. Every on-screen hint is therefore drawn on the side of the
//   button it refers to — LEFT-aligned text means the left button, RIGHT-aligned
//   means the right one. POSITION IS THE LABEL.
//
//   ⚠⚠ This was BACKWARDS on first flash: "SELECT" was drawn left-aligned while
//   SELECT is physically the right button. A tester held the left one for a while
//   and nothing happened — correct behaviour (OK has no hold gesture, it is a
//   strapping pin) reached through a hint pointing at the wrong button. Naming a
//   logical button on a device with two unlabelled physical ones is a 50/50
//   guess; placing the hint under the button is not.
static const int PIN_SELECT = 35;  // RIGHT button. input-only, EXTERNAL pull-up
static const int PIN_OK     = 0;   // LEFT button. strapping pin — short press only
static const int PIN_BL     = 4;   // backlight, LEDC

// ⚠ Set to -1 and every sound call compiles out — the board works with the wires
//   pulled, which is exactly what happens if the arriving case will not close
//   over them. GPIO21 is the repo's documented free pin on this board.
#define PB_BUZZER_PIN 21

static const uint16_t W = 240, H = 135;   // setRotation(1)

static const uint32_t DEBOUNCE_MS   = 60;
static const uint32_t REPEAT_WAIT_MS= 500;  // hold this long before auto-repeat
static const uint32_t REPEAT_FAST_MS= 220;  // then one step every…
static const uint32_t REPEAT_TURBO_MS=90;   // …accelerating after REPEAT_ACCEL_MS
static const uint32_t REPEAT_ACCEL_MS=2000;

static const uint32_t JUDGE_HOLD_MS = 1200; // CERTO / ERRADO dwell
static const uint32_t REVEAL_HOLD_MS= 2600; // the 3rd-try reveal
static const uint32_t BAD_HOLD_MS   = 1600; // a refused code, with its reason

// ⚠ DELIBERATE DEVIATION FROM THE 1988 TOY, DO NOT "FIX" IT BACK. The original
//   auto-powered-off after 3 minutes. Three minutes to read a printed question,
//   find the page and press through is not enough for this user, and a timeout
//   destroying 25 minutes of score is the worst thing this device can do.
static const uint32_t IDLE_MENU_MS  = 180000;  // 3 min — faithful
static const uint32_t IDLE_GAME_MS  = 600000;  // 10 min — deliberate

static const uint8_t BL_ON = 140;

// Colours. cyan = the current selection, and nothing else uses it.
#define C_BG      TFT_BLACK
#define C_TEXT    TFT_WHITE
#define C_DIM     0x8410
#define C_SEL     TFT_CYAN
#define C_RIGHT   0x07E0
#define C_WRONG   0xF800
#define C_TRY     0xFD20

// ---- state -----------------------------------------------------------------

enum Screen { SC_STANDBY, SC_CODE, SC_BAD, SC_QUESTION, SC_JUDGE, SC_SCORE };

TFT_eSPI tft = TFT_eSPI();
Preferences prefs;

static PbGame  game;
static Screen  screen      = SC_STANDBY;
static uint32_t screenAt   = 0;   // when the current screen was entered
static uint32_t lastInput  = 0;   // idle timer
static String  lastDrawn;         // render cache

static int  digits[3] = {0, 0, 0};// the three code positions, as VALUE INDEXES
static int  pos       = 0;        // which position is being edited
static int  answerSel = 0;        // 0..3 -> A..D
static PbVerdict lastVerdict = PB_IGNORED;
static const char *badReason = "";

static int  lastScore = -1;       // last completed session, for STANDBY
static int  lastBook = 0, lastSection = 0;
static bool muted = false;

// Value lists per code position. ⚠ The section digit gets SEVEN values, not ten:
// only 1-6 are legal, so offering 0/7/8/9 is offering a guaranteed refusal.
// Last entry of each list is the ERASE value.
static const char *VALS[3] = { "0123456789<", "0123456789<", "123456<" };
static int valCount(int p) { return (int)strlen(VALS[p]); }
static char valAt(int p, int i) { return VALS[p][i]; }

// ---- sound -----------------------------------------------------------------
//
// ⚠ NO delay() ANYWHERE IN HERE. A blocking jingle freezes the button scan and
//   the idle timer, and the 3-tries path is exactly where that hurts.

static const Note SFX_STEP[]   = { {1200, 25} };
static const Note SFX_OK[]     = { {1600, 40} };
static const Note SFX_RIGHT[]  = { {880, 90}, {1320, 140} };
static const Note SFX_WRONG[]  = { {200, 260} };
static const Note SFX_REVEAL[] = { {520, 110}, {0, 60}, {392, 220} };
static const Note SFX_BAD[]    = { {160, 90}, {0, 50}, {160, 90} };
/* Boot self-test. Its whole job is to answer "did the buzzer wiring work?" the
   instant the board powers up, with nothing to navigate to. Deliberately three
   rising notes so it cannot be confused with SFX_RIGHT (two notes) or any other
   cue. ⚠ It RESPECTS mute — a muted toy that chirps on every power-up is a bug,
   not a feature — so if you hear nothing, check the standby screen: it states
   the mute state in words. */
static const Note SFX_BOOT[]   = { {660, 70}, {880, 70}, {1320, 120} };
// The four end-of-session songs, one per score band — the toy really had four.
static const Note SONG_0[] = { {523,140},{659,140},{784,140},{1047,320} };           // OTIMO
static const Note SONG_1[] = { {523,150},{659,150},{784,300} };                      // MUITO BEM
static const Note SONG_2[] = { {440,170},{523,170},{440,300} };                      // QUASE LA
static const Note SONG_3[] = { {392,200},{330,200},{262,380} };                      // TENTE MAIS

static const Note *sq = nullptr;
static uint8_t sqLen = 0, sqIdx = 0;
static uint32_t sqAt = 0;
static bool sqOn = false;

static void soundStop()
{
#if PB_BUZZER_PIN >= 0
    ledcWriteTone(PB_BUZZER_PIN, 0);
#endif
    sq = nullptr; sqOn = false;
}

static void soundPlay(const Note *seq, uint8_t len)
{
#if PB_BUZZER_PIN >= 0
    if (muted) { return; }
    sq = seq; sqLen = len; sqIdx = 0; sqAt = 0; sqOn = false;
#else
    (void)seq; (void)len;
#endif
}
#define PLAY(x) soundPlay((x), (uint8_t)(sizeof(x) / sizeof((x)[0])))

static void soundTick()
{
#if PB_BUZZER_PIN >= 0
    if (!sq) { return; }
    uint32_t now = millis();
    if (!sqOn) {
        if (sqIdx >= sqLen) { soundStop(); return; }
        ledcWriteTone(PB_BUZZER_PIN, sq[sqIdx].hz);   // hz 0 = a rest
        sqAt = now; sqOn = true;
        return;
    }
    if (now - sqAt >= sq[sqIdx].ms) {
        ledcWriteTone(PB_BUZZER_PIN, 0);
        sqIdx++; sqOn = false;
    }
#endif
}

// ---- drawing helpers -------------------------------------------------------

static void centre(const char *s, int y)
{
    tft.setTextDatum(TC_DATUM);
    tft.drawString(s, W / 2, y);
}

static void small()  { tft.setFreeFont(&FreeSans9pt7b);      }
static void medium() { tft.setFreeFont(&FreeSansBold12pt7b); }
static void large()  { tft.setFreeFont(&FreeSansBold18pt7b); }
static void huge()   { tft.setFreeFont(&FreeSansBold24pt7b); }

/* ⚠ CLIPPING IS DESIGNED OUT, NOT HAND-TUNED. The first flash rendered
   "PENSE BEM" at 24pt and it ran off BOTH edges of the 240px panel — invisible
   to every host test, caught on the desk camera in one frame. Guessing a font
   size per string just moves the guess. This picks the largest face that
   MEASURES small enough for the width it is given, so a longer string degrades
   instead of losing its first and last letters.

   Same family as the font-6 scar this board already carries: the failure is
   silent and only glass shows it. */
static void centreFit(const char *s, int y, int maxW)
{
    huge();   if (tft.textWidth(s) <= maxW) { centre(s, y); return; }
    large();  if (tft.textWidth(s) <= maxW) { centre(s, y); return; }
    medium(); if (tft.textWidth(s) <= maxW) { centre(s, y); return; }
    small();  centre(s, y);
}

static const char *BANDS[4] = { "OTIMO", "MUITO BEM", "QUASE LA", "TENTE MAIS" };

// ---- screens ---------------------------------------------------------------

static void drawStandby()
{
    tft.fillScreen(C_BG);
    tft.setTextColor(C_TEXT, C_BG);
    centreFit("PENSE BEM", 20, W - 8);
    small();  tft.setTextColor(C_SEL, C_BG);
    centre("OK  PARA COMECAR", 78);
    /* ⚠ State first, then the verb. The old wording ("SELECT segurado = som OFF")
       read as a DESCRIPTION OF THE GESTURE rather than the current setting, which
       is ambiguous in the one moment it matters: silence with no explanation. */
    tft.setTextColor(muted ? C_WRONG : C_DIM, C_BG);
    centre(muted ? "SOM DESLIGADO" : "SOM LIGADO", 100);
    small(); tft.setTextColor(C_DIM, C_BG);
    tft.setTextDatum(TL_DATUM); tft.drawString("< OK comeca", 6, H - 20);
    tft.setTextDatum(TR_DATUM); tft.drawString("segure = som >", W - 6, H - 20);
    if (lastScore >= 0) {
        char buf[40];
        snprintf(buf, sizeof(buf), "ultimo  L%02d S%d  %d pts", lastBook, lastSection, lastScore);
        small(); tft.setTextColor(C_DIM, C_BG);
        centre(buf, 74);
    }
}

static void drawCode()
{
    tft.fillScreen(C_BG);
    small(); tft.setTextColor(C_DIM, C_BG);
    tft.setTextDatum(TL_DATUM); tft.drawString("CODIGO DO LIVRO", 6, 4);
    tft.setTextDatum(TR_DATUM);
    tft.drawString(pos == 2 ? "SECAO" : "LIVRO", W - 6, 4);
    tft.drawFastHLine(0, 26, W, C_DIM);

    for (int i = 0; i < 3; i++) {
        int x = 46 + i * 56;
        huge();
        if (i < pos) {
            tft.setTextColor(C_TEXT, C_BG);
            char c[2] = { valAt(i, digits[i]), 0 };
            tft.setTextDatum(TC_DATUM); tft.drawString(c, x, 40);
        } else if (i == pos) {
            char v = valAt(i, digits[i]);
            tft.drawRect(x - 24, 34, 48, 52, C_SEL);
            tft.setTextColor(C_SEL, C_BG);
            char c[2] = { v == '<' ? '<' : v, 0 };
            tft.setTextDatum(TC_DATUM); tft.drawString(c, x, 40);
        } else {
            tft.setTextColor(C_DIM, C_BG);
            tft.setTextDatum(TC_DATUM); tft.drawString("-", x, 40);
        }
    }
    small(); tft.setTextColor(C_DIM, C_BG);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(valAt(pos, digits[pos]) == '<' ? "< OK apaga" : "< OK confirma", 6, H - 20);
    tft.setTextDatum(TR_DATUM); tft.drawString("muda >", W - 6, H - 20);
}

static void drawBad()
{
    tft.fillScreen(C_BG);
    tft.setTextColor(C_WRONG, C_BG);
    centreFit("CODIGO INVALIDO", 28, W - 8);
    tft.setTextColor(C_TEXT, C_BG);
    centreFit(badReason, 76, W - 8);
}

static void drawQuestion()
{
    char buf[32];
    tft.fillScreen(C_BG);
    small();
    tft.setTextColor(C_DIM, C_BG);
    tft.setTextDatum(TL_DATUM);
    snprintf(buf, sizeof(buf), "Q%02d/%02d", game.index + 1, game.n);
    tft.drawString(buf, 6, 4);

    // ⚠ THE ABSOLUTE QUESTION NUMBER IS LOAD-BEARING. Q07/30 is where you are in
    //   the session; #067 is the number to look up in the printed book. Without
    //   it section 6 (scattered questions) is unusable — and the answer is a
    //   function of the absolute number in EVERY section.
    tft.setTextColor(C_TEXT, C_BG);
    tft.setTextDatum(TC_DATUM);
    snprintf(buf, sizeof(buf), "#%03d", pb_game_question(&game));
    tft.drawString(buf, W / 2, 4);

    tft.setTextColor(game.attempt == 1 ? C_DIM : C_TRY, C_BG);
    tft.setTextDatum(TR_DATUM);
    snprintf(buf, sizeof(buf), "TENT %d/3", game.attempt);
    tft.drawString(buf, W - 6, 4);
    tft.drawFastHLine(0, 26, W, C_DIM);

    for (int i = 0; i < 4; i++) {
        int x = 8 + i * 58, y = 44, w = 50, h = 54;
        char c[2] = { (char)('A' + i), 0 };
        huge();
        if (i == answerSel) {
            tft.fillRect(x, y, w, h, C_SEL);
            tft.setTextColor(C_BG, C_SEL);
        } else {
            tft.drawRect(x, y, w, h, C_DIM);
            tft.setTextColor(C_TEXT, C_BG);
        }
        tft.setTextDatum(TC_DATUM);
        tft.drawString(c, x + w / 2, y + 6);
    }
    small(); tft.setTextColor(C_DIM, C_BG);
    tft.setTextDatum(TL_DATUM); tft.drawString("< OK responde", 6, H - 18);
    tft.setTextDatum(TR_DATUM); tft.drawString("troca >", W - 6, H - 18);
}

static void drawJudge()
{
    char buf[32];
    tft.fillScreen(C_BG);
    tft.setTextDatum(TC_DATUM);

    if (lastVerdict == PB_RIGHT) {
        tft.setTextColor(C_RIGHT, C_BG);
        centreFit("CERTO!", 32, W - 8);
        small(); tft.setTextColor(C_TEXT, C_BG);
        snprintf(buf, sizeof(buf), "+%d pontos", pb_game_last_points(&game));
        centre(buf, 92);
    } else if (lastVerdict == PB_RETRY) {
        tft.setTextColor(C_WRONG, C_BG);
        centreFit("ERRADO", 30, W - 8);
        tft.setTextColor(C_TRY, C_BG);
        snprintf(buf, sizeof(buf), "tentativa %d de 3", game.attempt);
        centreFit(buf, 88, W - 8);
    } else {
        tft.setTextColor(C_WRONG, C_BG);
        medium(); centre("ERRADO", 8);
        tft.setTextColor(C_TEXT, C_BG);
        small(); centre("a certa era", 46);
        huge(); tft.setTextColor(C_TRY, C_BG);
        char c[2] = { pb_game_revealed(&game), 0 };
        centre(c, 68);
    }
}

static void drawScore()
{
    char buf[40];
    int norm = pb_game_normalized(&game);
    int band = pb_game_band(&game);
    tft.fillScreen(C_BG);
    small(); tft.setTextColor(C_DIM, C_BG);
    tft.setTextDatum(TL_DATUM);
    snprintf(buf, sizeof(buf), "FIM   LIVRO %02d  SECAO %d", game.book, game.section);
    tft.drawString(buf, 6, 4);
    tft.drawFastHLine(0, 26, W, C_DIM);

    tft.setTextColor(C_TEXT, C_BG);
    snprintf(buf, sizeof(buf), "%d", game.raw);
    centreFit(buf, 34, W - 8);
    small(); tft.setTextColor(C_DIM, C_BG);
    snprintf(buf, sizeof(buf), "de 300      %d%%", norm);
    centre(buf, 88);
    tft.setTextColor(band == 0 ? C_RIGHT : (band == 3 ? C_WRONG : C_TRY), C_BG);
    centreFit(BANDS[band], 104, W - 8);
    tft.fillRect(0, H - 5, (int)((long)W * norm / 100), 5,
                 band == 0 ? C_RIGHT : (band == 3 ? C_WRONG : C_TRY));
}

// Repaint only when something actually changed — a full 240x135 push is not free.
static void render()
{
    char sig[128];
    snprintf(sig, sizeof(sig), "%d|%d%d%d|%d|%d|%d|%d|%d|%d|%d|%d",
             (int)screen, digits[0], digits[1], digits[2], pos, answerSel,
             game.index, game.attempt, game.raw, (int)lastVerdict,
             lastScore, muted ? 1 : 0);
    if (lastDrawn == sig) { return; }
    lastDrawn = sig;

    switch (screen) {
    case SC_STANDBY:  drawStandby();  break;
    case SC_CODE:     drawCode();     break;
    case SC_BAD:      drawBad();      break;
    case SC_QUESTION: drawQuestion(); break;
    case SC_JUDGE:    drawJudge();    break;
    case SC_SCORE:    drawScore();    break;
    }
}

static void go(Screen s)
{
    screen = s;
    screenAt = millis();
    lastDrawn = "";
}

// ---- actions ---------------------------------------------------------------

static void startCode()
{
    digits[0] = digits[1] = 0;
    digits[2] = 0;                 // VALS[2] is "123456<", so index 0 is section 1
    pos = 0;
    go(SC_CODE);
}

static void commitCode()
{
    char v = valAt(pos, digits[pos]);

    if (v == '<') {                        // ERASE is a value, never a long press
        if (pos == 0) { go(SC_STANDBY); }
        else          { pos--; go(SC_CODE); }
        PLAY(SFX_STEP);
        return;
    }
    if (pos < 2) { pos++; go(SC_CODE); PLAY(SFX_OK); return; }

    {
        int book = (valAt(0, digits[0]) - '0') * 10 + (valAt(1, digits[1]) - '0');
        int section = valAt(2, digits[2]) - '0';

        if (book < 1) {
            badReason = "livro 00 nao existe";
            PLAY(SFX_BAD); go(SC_BAD); return;
        }
        // ⚠ Seeded from micros() at the confirm press. esp_random() is only a true
        //   RNG while RF is active, and this game must be correct with WiFi down;
        //   a human's press timing is real entropy and costs one line.
        if (!pb_game_start(&game, book, section, (unsigned long long)micros())) {
            badReason = "codigo invalido";
            PLAY(SFX_BAD); go(SC_BAD); return;
        }
        answerSel = 0;
        PLAY(SFX_OK);
        go(SC_QUESTION);
    }
}

static void submitAnswer()
{
    lastVerdict = pb_game_answer(&game, (char)('A' + answerSel));

    // ⚠ PB_IGNORED changes nothing and must never look like a wrong answer.
    if (lastVerdict == PB_IGNORED) { return; }

    if (lastVerdict == PB_RIGHT)      { PLAY(SFX_RIGHT);  }
    else if (lastVerdict == PB_RETRY) { PLAY(SFX_WRONG);  }
    else                              { PLAY(SFX_REVEAL); }

    answerSel = 0;
    go(SC_JUDGE);
}

static void afterJudge()
{
    if (game.done) {
        lastScore = game.raw;
        lastBook = game.book;
        lastSection = game.section;
        prefs.putInt("score", lastScore);
        prefs.putInt("book", lastBook);
        prefs.putInt("sec", lastSection);
        switch (pb_game_band(&game)) {
        case 0: PLAY(SONG_0); break;
        case 1: PLAY(SONG_1); break;
        case 2: PLAY(SONG_2); break;
        default: PLAY(SONG_3); break;
        }
        go(SC_SCORE);
    } else {
        go(SC_QUESTION);
    }
}

static void onSelect()
{
    lastInput = millis();
    switch (screen) {
    case SC_CODE:
        digits[pos] = (digits[pos] + 1) % valCount(pos);
        PLAY(SFX_STEP);
        break;
    case SC_QUESTION:
        answerSel = (answerSel + 1) % 4;
        PLAY(SFX_STEP);
        break;
    default:
        break;
    }
}

static void onOk()
{
    lastInput = millis();
    switch (screen) {
    case SC_STANDBY:  PLAY(SFX_OK); startCode();  break;
    case SC_CODE:     commitCode();               break;
    case SC_QUESTION: submitAnswer();             break;
    case SC_SCORE:    PLAY(SFX_OK); go(SC_STANDBY); break;
    default:          break;   // SC_BAD and SC_JUDGE time out on their own
    }
}

// ---- input -----------------------------------------------------------------

static void inputTick()
{
    static bool okDown = false, selDown = false;
    static uint32_t okAt = 0, selAt = 0, nextRepeat = 0;

    uint32_t now = millis();
    bool ok  = (digitalRead(PIN_OK)     == LOW);
    bool sel = (digitalRead(PIN_SELECT) == LOW);

    // OK: fires on RELEASE, after a debounce. No hold gesture — GPIO0 is a
    // strapping pin and the sibling board's watchdog scar lives on holding it.
    if (ok && !okDown) { okDown = true; okAt = now; }
    else if (!ok && okDown) {
        okDown = false;
        if (now - okAt >= DEBOUNCE_MS) { onOk(); }
    }

    // SELECT: fires on PRESS, then auto-repeats while held (accelerating).
    // That is what makes 0-9 entry painless with a single button.
    if (sel && !selDown) {
        selDown = true; selAt = now;
        onSelect();
        nextRepeat = now + REPEAT_WAIT_MS;
    } else if (sel && selDown) {
        if (now >= nextRepeat) {
            // A long hold on STANDBY toggles mute instead of repeating.
            if (screen == SC_STANDBY) {
                muted = !muted;
                prefs.putBool("muted", muted);
                if (!muted) { PLAY(SFX_OK); }
                lastDrawn = "";
                nextRepeat = now + 1000;
            } else {
                onSelect();
                nextRepeat = now + ((now - selAt > REPEAT_ACCEL_MS) ? REPEAT_TURBO_MS
                                                                   : REPEAT_FAST_MS);
            }
        }
    } else if (!sel && selDown) {
        selDown = false;
    }
}

// ---- timers ----------------------------------------------------------------

static void timerTick()
{
    uint32_t now = millis();

    if (screen == SC_JUDGE) {
        uint32_t hold = (lastVerdict == PB_REVEAL) ? REVEAL_HOLD_MS : JUDGE_HOLD_MS;
        if (now - screenAt >= hold) { afterJudge(); }
        return;
    }
    if (screen == SC_BAD && now - screenAt >= BAD_HOLD_MS) { startCode(); return; }

    // ⚠ The game screen gets 10 minutes, the menus 3. See the constants block.
    uint32_t limit = (screen == SC_QUESTION) ? IDLE_GAME_MS : IDLE_MENU_MS;
    if (screen != SC_STANDBY && now - lastInput >= limit) { go(SC_STANDBY); }
}

// ---- setup / loop ----------------------------------------------------------

void setup()
{
    Serial.begin(115200);

    pinMode(PIN_OK, INPUT_PULLUP);
    pinMode(PIN_SELECT, INPUT);   // ⚠ input-only pin: no internal pull-up exists

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(C_BG);

    // ⚠ AFTER tft.init(): it re-claims its backlight pin as a plain output and
    //   would clobber a PWM mux attached before it.
    ledcAttach(PIN_BL, 5000, 8);
    ledcWrite(PIN_BL, BL_ON);
#if PB_BUZZER_PIN >= 0
    ledcAttach(PB_BUZZER_PIN, 2000, 10);
    ledcWriteTone(PB_BUZZER_PIN, 0);
#endif

    pb_init();

    prefs.begin("pensebem", false);
    muted       = prefs.getBool("muted", false);
    lastScore   = prefs.getInt("score", -1);
    lastBook    = prefs.getInt("book", 0);
    lastSection = prefs.getInt("sec", 0);

    // WiFi is OTA-only and NEVER blocks. The game must work with the AP down.
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    MDNS.begin(HOSTNAME);
    ArduinoOTA.setHostname(HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASS);
    ArduinoOTA.onStart([]() { soundStop(); tft.fillScreen(C_BG);
                              medium(); tft.setTextColor(C_SEL, C_BG);
                              centre("ATUALIZANDO", 50); });
    ArduinoOTA.onEnd([]()   { centre("OK", 90); });
    ArduinoOTA.begin();

    // Serial parity dump — diff against the Go backend's
    // GET /api/v1/books/{n}/answers. Two implementations agreeing is a real
    // check; a header agreeing with itself is not.
    Serial.printf("\npense-bem up. offsets popped=%d (must be %d)\n",
                  pb_offsets_popped(), PB_OFFSETS);
    for (int b = 1; b <= 99; b += 49) {
        Serial.printf("book %02d: ", b);
        const unsigned char *k = pb_answer_key(b);
        for (int q = 0; q < PB_QUESTIONS; q++) { Serial.write(k[q]); }
        Serial.println();
    }

    lastInput = millis();
    go(SC_STANDBY);
    PLAY(SFX_BOOT);   /* buzzer wiring self-test — see SFX_BOOT */
}

void loop()
{
    ArduinoOTA.handle();
    inputTick();
    timerTick();
    soundTick();
    render();
    delay(10);
}

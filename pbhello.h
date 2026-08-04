#pragma once
/* pbhello.h — the "say hello once" payload, as pure functions.
 *
 * ⚠ WHAT THIS DEVICE SENDS, AND WHY IT SENDS ANYTHING AT ALL.
 *
 * The board reports its existence once per boot: a random identifier it made up
 * itself, and the firmware build stamp. Nothing else. Not the book, not the
 * score, not an answer, not a nickname, not the MAC address.
 *
 * It exists because the author set a gate before publishing this project — 50
 * GitHub stars and 5 boards actually flashed and switched on — and wanted the
 * second number measured rather than self-reported. That is the whole purpose.
 *
 * ⚠ NEVER THE MAC ADDRESS. A MAC is assigned hardware identity: it follows a
 * person across every network they join, and it is the same value their router,
 * their ISP and every captive portal already sees. A random UUID made on first
 * boot counts a device without identifying anybody. These two things look
 * equally like "an id" in a payload and are not remotely the same object.
 *
 * ⚠ CONSENT IS VERSIONED, NOT A BOOLEAN — and that is a correction, not a
 * flourish. The first draft of the on-screen disclosure said "nunca suas
 * respostas" (never your answers). The planned multiplayer follow-up has a
 * public scoreboard, which is precisely answers leaving the device. A promise a
 * later version breaks is worse than no promise, so: the stored consent records
 * WHICH PAYLOAD it was given for. When the payload changes, the old consent no
 * longer covers it and the board must disclose again. The mechanism keeps the
 * promise; wording alone could not.
 *
 * This header is pure — no Arduino headers, no I/O — so hello-test/ compiles it
 * on the host and the sketch #includes the same file. No mirror, no drift.
 */

#include <stddef.h>

/* The payload shape this firmware sends. BUMP THIS whenever the payload gains
   or changes a field, and every board will re-disclose before sending again.

   v2 (2026-08-04) — the request gained an X-PB-Sign header: HMAC-SHA256 of the
   body under a shared key, so the counter can tell a real build from a curl.
   ⚠ BUMPED EVEN THOUGH THE SIGNATURE REVEALS NOTHING NEW — it is a function of
   the two fields already disclosed, and carries no further fact about the
   device or its owner. It is bumped anyway because the rule is "the payload
   changed", not "I judged the change harmless", and a consent mechanism whose
   trigger is somebody's judgement is not a mechanism. */
#define PB_HELLO_PAYLOAD_VERSION 2

/* 36 characters plus a terminator. */
#define PB_HELLO_UUID_LEN 37

/* Bounded so the sketch can stack-allocate and the server can cap its reader. */
#define PB_HELLO_MAX_VERSION 32
#define PB_HELLO_MAX_PAYLOAD 128

/* pb_hello_uuid4 formats 16 bytes of entropy as a canonical lowercase v4 UUID.
 *
 * ⚠ The version and variant bits are FORCED, not hoped for. A server that
 * validates strictly (ours does) rejects a UUID whose 13th nibble is not 4 or
 * whose 17th is not 8/9/a/b — so a board that skipped this would be silently
 * uncountable, which is the one failure mode nobody would notice: the device
 * works perfectly and simply never appears in the number.
 *
 * out must have room for PB_HELLO_UUID_LEN bytes. */
static inline void pb_hello_uuid4(const unsigned char rnd[16], char *out)
{
    unsigned char b[16];
    for (int i = 0; i < 16; i++) b[i] = rnd[i];

    b[6] = (unsigned char)((b[6] & 0x0F) | 0x40);  /* version 4  */
    b[8] = (unsigned char)((b[8] & 0x3F) | 0x80);  /* variant 10 */

    static const char *hex = "0123456789abcdef";
    int o = 0;
    for (int i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10) out[o++] = '-';
        out[o++] = hex[(b[i] >> 4) & 0x0F];
        out[o++] = hex[b[i] & 0x0F];
    }
    out[o] = '\0';
}

/* pb_hello_valid reports whether a string is a canonical lowercase v4 UUID —
 * the exact shape the counter accepts.
 *
 * Used on boot to check what came back out of NVS. Flash can be corrupted, a
 * key can collide with an older firmware's, and a half-written string would
 * otherwise be POSTed forever and rejected every time: the board would work
 * perfectly and never be counted, with nothing on screen to say so.
 *
 * ⚠ hello-test/ deliberately keeps its OWN, separately written copy of the
 * server's rule rather than calling this. Two implementations agreeing is
 * evidence; a function agreeing with itself is a tautology. */
static inline int pb_hello_valid(const char *s)
{
    if (!s) return 0;
    size_t n = 0;
    while (s[n]) { if (n > 36) return 0; n++; }
    if (n != 36) return 0;

    for (size_t i = 0; i < 36; i++) {
        char c = s[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-') return 0;
            continue;
        }
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return 0;
    }
    if (s[14] != '4') return 0;                                   /* version */
    if (s[19] != '8' && s[19] != '9' && s[19] != 'a' && s[19] != 'b')
        return 0;                                                 /* variant */
    return 1;
}

/* pb_hello_sanitize copies a build stamp, keeping only characters the server
 * accepts: [A-Za-z0-9._-]. Anything else is dropped.
 *
 * ⚠ A TAB OR A NEWLINE HERE WOULD FORGE A ROW. The server stores one board per
 * line as "uuid<TAB>version", so an unsanitised stamp containing either could
 * inject a second record. `git describe` never produces one — which is exactly
 * why this is worth enforcing rather than assuming: the day something else
 * generates that string, nobody will remember this constraint existed.
 *
 * Truncates to PB_HELLO_MAX_VERSION-1 characters. Returns the length written. */
static inline size_t pb_hello_sanitize(const char *in, char *out, size_t outCap)
{
    size_t n = 0;
    if (outCap == 0) return 0;
    for (const char *p = in; p && *p && n + 1 < outCap; p++) {
        char c = *p;
        int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                 (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        if (ok) out[n++] = c;
    }
    out[n] = '\0';
    return n;
}

/* pb_hello_payload builds the exact JSON body sent to the counter.
 *
 * ⚠ TWO FIELDS. If a third ever appears here, PB_HELLO_PAYLOAD_VERSION must be
 * bumped in the same commit, or boards will send something their owner was
 * never shown. A host test asserts the body has exactly these two keys.
 *
 * Returns the length written, or 0 if it would not fit (in which case nothing
 * is sent — a truncated body is not a smaller disclosure, it is a broken one). */
static inline size_t pb_hello_payload(const char *uuid, const char *version,
                                      char *out, size_t outCap)
{
    char safe[PB_HELLO_MAX_VERSION];
    pb_hello_sanitize(version, safe, sizeof safe);

    const char *a = "{\"id\":\"";
    const char *b = "\",\"v\":\"";
    const char *c = "\"}";

    size_t need = 0;
    for (const char *p = a; *p; p++) need++;
    for (const char *p = uuid; *p; p++) need++;
    for (const char *p = b; *p; p++) need++;
    for (const char *p = safe; *p; p++) need++;
    for (const char *p = c; *p; p++) need++;

    if (need + 1 > outCap) return 0;

    size_t o = 0;
    for (const char *p = a;    *p; p++) out[o++] = *p;
    for (const char *p = uuid; *p; p++) out[o++] = *p;
    for (const char *p = b;    *p; p++) out[o++] = *p;
    for (const char *p = safe; *p; p++) out[o++] = *p;
    for (const char *p = c;    *p; p++) out[o++] = *p;
    out[o] = '\0';
    return o;
}

/* ── the disclosure text ───────────────────────────────────────────────────
 * ⚠ LIVES HERE SO THE SKETCH AND hello-test/fit.c SHARE ONE COPY. A test that
 * re-types the strings it is checking proves only that someone typed them twice.
 *
 * ⚠ EVERY LINE MUST FIT 232 px AT FreeSans9pt. centreFit() steps a string down
 * through font sizes, but T_SMALL is the floor — below it there is nothing, so
 * an over-wide body line does not shrink, it CLIPS at the panel edge. Three of
 * these did, including "how to turn it off", in both languages. fit.c measures
 * every one against the real TFT_eSPI font tables and fails under 12 px of
 * margin, because a one-word edit can otherwise put a line over the edge with
 * nothing to notice until someone looks at hardware.
 *
 * Index 0 is the title (drawn at T_MED, which falls back to 9pt on its own).
 * Indices 1.. are body lines at T_SMALL and have no fallback at all.
 *
 * ⚠ "PB_PHONE_HOME 0" GETS ITS OWN LINE, and that is measurement, not taste:
 * the token alone is ~170 px at 9pt, so ANY prefix on the same line pushes it
 * past 232 and clips. Every attempt to keep it on one line with a verb failed
 * the measurement. Splitting it is what lets the server URL stay on screen too,
 * and a disclosure that does not say where it sends is not a disclosure. */
#define PB_HELLO_LINES 7

static const char *const PB_HELLO_PT[PB_HELLO_LINES] = {
    "AVISA UMA VEZ",
    "numero aleatorio, versao",
    "e uma assinatura",
    "nada mais, sem cripto",
    "api.pense-bem-wars.com",
    "para desligar:",
    "PB_PHONE_HOME 0",
};

static const char *const PB_HELLO_EN[PB_HELLO_LINES] = {
    "SAYS HELLO",
    "sends a random number,",
    "the version and a signature",
    "nothing else, not encrypted",
    "api.pense-bem-wars.com",
    "to turn it off:",
    "PB_PHONE_HOME 0",
};

/* ⚠ "sem cripto" / "not encrypted" IS STILL TRUE AND MUST STAY TRUE. A
   signature authenticates; it does not encrypt. The body still crosses the
   network in plain text, readable by anyone on the path, exactly as before —
   so the line that says so is not weakened by the line above it. If this ever
   becomes HTTPS, change that line in the same commit. */

/* pb_hello_needs_disclosure decides whether the board must show the notice
 * before sending. Stored consent below the current payload version does not
 * cover it — including the first boot, where nothing is stored (0). */
static inline int pb_hello_needs_disclosure(int storedConsentVersion)
{
    return storedConsentVersion < PB_HELLO_PAYLOAD_VERSION;
}

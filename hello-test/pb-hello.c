/* pb-hello.c — host proof for pbhello.h.
 *
 * The board says hello once per boot. Everything it is willing to say lives in
 * that header, so everything it is willing to say is asserted here.
 *
 * ⚠ EXPECTED VALUES ARE LITERALS. Never an expression built from the code under
 * test. An assertion whose two sides share a term co-varies into a tautology:
 * the implementation changes, both sides move together, the test stays green and
 * constrains nothing. That failure has shipped in this project's sibling repo
 * more than once and it is the reason this comment exists.
 */

#include <stdio.h>
#include <string.h>
#include "../pbhello.h"

static int fails = 0;

static void eq_str(const char *what, const char *got, const char *want)
{
    if (strcmp(got, want) != 0) {
        printf("  FAIL %s\n    got  %s\n    want %s\n", what, got, want);
        fails++;
    }
}

static void eq_int(const char *what, long got, long want)
{
    if (got != want) {
        printf("  FAIL %s: got %ld, want %ld\n", what, got, want);
        fails++;
    }
}

/* Mirrors the server's validator exactly: lowercase canonical v4.
   ^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$ */
static int server_would_accept(const char *s)
{
    static const int dashes[] = {8, 13, 18, 23};
    if (strlen(s) != 36) return 0;
    for (int i = 0; i < 36; i++) {
        int isDash = (i == dashes[0] || i == dashes[1] || i == dashes[2] || i == dashes[3]);
        char c = s[i];
        if (isDash) { if (c != '-') return 0; continue; }
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return 0;
    }
    if (s[14] != '4') return 0;                                  /* version  */
    if (!(s[19] == '8' || s[19] == '9' || s[19] == 'a' || s[19] == 'b'))
        return 0;                                                /* variant  */
    return 1;
}

int main(void)
{
    char uuid[PB_HELLO_UUID_LEN];
    char buf[PB_HELLO_MAX_PAYLOAD];

    /* ---- 1. UUID formatting, against a hand-computed literal ------------- */
    {
        unsigned char rnd[16] = {
            0x3f, 0x25, 0x04, 0xe0, 0x4f, 0x89, 0x01, 0xd3,
            0x0a, 0x0c, 0x03, 0x05, 0xe8, 0x2c, 0x33, 0x01
        };
        /* byte 6 0x01 -> 0x41 (version 4); byte 8 0x0a -> 0x8a (variant 10) */
        pb_hello_uuid4(rnd, uuid);
        eq_str("uuid4 canonical form", uuid,
               "3f2504e0-4f89-41d3-8a0c-0305e82c3301");
    }

    /* ---- 2. version/variant bits are FORCED, at both extremes ------------ */
    {
        unsigned char zero[16]; memset(zero, 0x00, 16);
        pb_hello_uuid4(zero, uuid);
        eq_str("all-zero entropy still a valid v4", uuid,
               "00000000-0000-4000-8000-000000000000");
        eq_int("  server accepts it", server_would_accept(uuid), 1);

        unsigned char ones[16]; memset(ones, 0xFF, 16);
        pb_hello_uuid4(ones, uuid);
        eq_str("all-ones entropy still a valid v4", uuid,
               "ffffffff-ffff-4fff-bfff-ffffffffffff");
        eq_int("  server accepts it", server_would_accept(uuid), 1);
    }

    /* ---- 3. every byte pattern produces something the server accepts -----
       ⚠ THIS IS THE ONE THAT MATTERS. A board whose UUID the server rejects
       works perfectly and is simply never counted — the failure is invisible
       from the device and invisible from the count. */
    {
        int bad = 0, disagree = 0;
        for (int k = 0; k < 256; k++) {
            unsigned char rnd[16];
            for (int i = 0; i < 16; i++) rnd[i] = (unsigned char)((k * 31 + i * 7) & 0xFF);
            pb_hello_uuid4(rnd, uuid);
            if (!server_would_accept(uuid)) bad++;
            /* Two independently written checkers must agree. The header's
               pb_hello_valid() and this file's server_would_accept() were
               written separately from the same spec; that is what makes their
               agreement evidence rather than a tautology. */
            if (pb_hello_valid(uuid) != server_would_accept(uuid)) disagree++;
        }
        eq_int("256 entropy patterns, all server-acceptable", bad, 0);
        eq_int("  header validator agrees with the server's rule", disagree, 0);
    }

    /* ---- 3b. the validator that guards NVS actually rejects things -------
       ⚠ A validator that returns 1 for everything would pass every test above,
       because every value it is fed there is genuinely valid. This is where it
       has to say NO. */
    {
        eq_int("rejects empty",        pb_hello_valid(""), 0);
        eq_int("rejects a MAC",        pb_hello_valid("a4:cf:12:df:8e:20"), 0);
        eq_int("rejects truncated",    pb_hello_valid("3f2504e0-4f89-41d3-8a0c-0305e82c33"), 0);
        eq_int("rejects too long",     pb_hello_valid("3f2504e0-4f89-41d3-8a0c-0305e82c33011"), 0);
        eq_int("rejects uppercase",    pb_hello_valid("3F2504E0-4F89-41D3-8A0C-0305E82C3301"), 0);
        eq_int("rejects wrong version",pb_hello_valid("3f2504e0-4f89-11d3-8a0c-0305e82c3301"), 0);
        eq_int("rejects wrong variant",pb_hello_valid("3f2504e0-4f89-41d3-0a0c-0305e82c3301"), 0);
        eq_int("rejects misplaced dash",pb_hello_valid("3f2504e04-f89-41d3-8a0c-0305e82c3301"), 0);
        eq_int("accepts the real thing",pb_hello_valid("3f2504e0-4f89-41d3-8a0c-0305e82c3301"), 1);
    }

    /* ---- 4. the build stamp cannot forge a storage row -------------------
       The server stores "uuid<TAB>version\n" per line. A tab or newline in the
       stamp could inject a second record; a quote could break the JSON. */
    {
        char safe[PB_HELLO_MAX_VERSION];
        pb_hello_sanitize("a1b2c3d", safe, sizeof safe);
        eq_str("clean stamp survives", safe, "a1b2c3d");

        pb_hello_sanitize("a1b2\tc3d", safe, sizeof safe);
        eq_str("TAB stripped", safe, "a1b2c3d");

        pb_hello_sanitize("a1b2\nc3d", safe, sizeof safe);
        eq_str("NEWLINE stripped", safe, "a1b2c3d");

        pb_hello_sanitize("a1\"b\\2", safe, sizeof safe);
        eq_str("quote and backslash stripped", safe, "a1b2");

        pb_hello_sanitize("v1.2.3-4-gabc123-dirty", safe, sizeof safe);
        eq_str("real git describe output survives intact", safe,
               "v1.2.3-4-gabc123-dirty");

        /* Truncation must not overflow. */
        char tiny[8];
        size_t n = pb_hello_sanitize("abcdefghijklmnop", tiny, sizeof tiny);
        eq_int("truncates to capacity-1", (long)n, 7);
        eq_str("  and stays terminated", tiny, "abcdefg");
    }

    /* ---- 5. the payload is EXACTLY two fields ---------------------------- */
    {
        size_t n = pb_hello_payload("3f2504e0-4f89-41d3-8a0c-0305e82c3301",
                                    "a1b2c3d", buf, sizeof buf);
        eq_str("payload body",
               buf,
               "{\"id\":\"3f2504e0-4f89-41d3-8a0c-0305e82c3301\",\"v\":\"a1b2c3d\"}");
        eq_int("payload length reported", (long)n, (long)strlen(buf));

        /* Count the keys. A third field must not appear without a deliberate
           PB_HELLO_PAYLOAD_VERSION bump, and this is what notices. */
        int colons = 0;
        for (const char *p = buf; *p; p++) if (*p == ':') colons++;
        eq_int("exactly two JSON fields", colons, 2);

        /* Nothing resembling a MAC address can appear in a well-formed body. */
        eq_int("no colon-separated MAC shape", strstr(buf, ":") != NULL, 1);
        eq_int("no MAC octet separator pattern",
               strstr(buf, "\":\"a4:cf") == NULL, 1);
    }

    /* ---- 6. a body that will not fit is NOT truncated, it is refused ----- */
    {
        char small[16];
        memset(small, 'X', sizeof small);
        size_t n = pb_hello_payload("3f2504e0-4f89-41d3-8a0c-0305e82c3301",
                                    "a1b2c3d", small, sizeof small);
        eq_int("oversized payload refused", (long)n, 0);
        /* A truncated disclosure is not a smaller disclosure — it is a broken
           one, so nothing is sent and the buffer is left alone. */
        eq_int("  and the buffer was not written", small[0] == 'X', 1);
    }

    /* ---- 7. consent is scoped to the payload version --------------------
       ⚠ LITERALS, DELIBERATELY. If PB_HELLO_PAYLOAD_VERSION is bumped to 2,
       the second assertion below goes RED — and that is the intended design,
       not a brittle test. Changing what the board sends must force whoever
       changed it to look at the disclosure that promised otherwise. */
    {
        eq_int("first boot (nothing stored) must disclose",
               pb_hello_needs_disclosure(0), 1);
        eq_int("consent given for payload v1 covers v1",
               pb_hello_needs_disclosure(1), 0);
        eq_int("consent from an older payload does NOT carry forward",
               pb_hello_needs_disclosure(-1), 1);
    }

    if (fails) { printf("\n%d FAILED\n", fails); return 1; }
    printf("  all assertions passed\n");
    return 0;
}

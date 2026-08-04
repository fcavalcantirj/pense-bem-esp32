/* pb-hmac.c — host proof for pbhmac.h.
 *
 * ⚠ EVERY EXPECTED DIGEST BELOW IS A PUBLISHED CONSTANT, PASTED, NOT COMPUTED.
 *
 * The tempting test — hash something, hash it again, assert they match — is a
 * tautology: both sides run the same code, so a completely wrong SHA-256 passes
 * it. Only a third-party constant can disagree with this implementation. The Go
 * server is checked against the SAME RFC 4231 vectors, so the two ends agreeing
 * is evidence rather than a coincidence of shared authorship.
 *
 * Sources:
 *   SHA-256 vectors — FIPS 180-2 / NIST examples
 *   HMAC-SHA-256 vectors — RFC 4231 §4
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../pbhmac.h"

static int failures = 0;

static void hex_of(const uint8_t *d, size_t n, char *out)
{
    static const char h[] = "0123456789abcdef";
    size_t i;
    for (i = 0; i < n; i++) { out[i * 2] = h[d[i] >> 4]; out[i * 2 + 1] = h[d[i] & 15]; }
    out[n * 2] = '\0';
}

static void check(const char *name, const char *got, const char *want)
{
    if (strcmp(got, want) == 0) {
        printf("   \033[32mok\033[0m   %s\n", name);
    } else {
        printf("   \033[31mFAIL\033[0m %s\n        got  %s\n        want %s\n", name, got, want);
        failures++;
    }
}

static void sha_case(const char *name, const uint8_t *msg, size_t len, const char *want)
{
    pb_sha256_ctx c;
    uint8_t d[PB_SHA256_DIGEST];
    char got[PB_SHA256_DIGEST * 2 + 1];
    pb_sha256_init(&c);
    pb_sha256_update(&c, msg, len);
    pb_sha256_final(&c, d);
    hex_of(d, PB_SHA256_DIGEST, got);
    check(name, got, want);
}

static void hmac_case(const char *name, const uint8_t *k, size_t kl,
                      const uint8_t *m, size_t ml, const char *want)
{
    char got[PB_HMAC_HEX_LEN];
    size_t n = pb_hmac_hex(k, kl, m, ml, got, sizeof got);
    if (n != 64) {
        printf("   \033[31mFAIL\033[0m %s — pb_hmac_hex wrote %zu chars, want 64\n", name, n);
        failures++;
        return;
    }
    check(name, got, want);
}

int main(void)
{
    uint8_t buf[200];
    size_t i;

    puts("== SHA-256 against NIST vectors ==");
    /* ⚠ THE 55/56/64-BYTE CASES ARE THE PADDING BRANCH, and padding is where a
       hand-rolled SHA-256 is wrong if it is wrong anywhere: at 56 bytes the
       length field no longer fits in the block and a second one must be
       compressed. Nothing in this firmware's real payload (~90 bytes) would
       exercise the boundary by accident. */
    sha_case("empty string", (const uint8_t *)"", 0,
             "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    sha_case("abc", (const uint8_t *)"abc", 3,
             "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    sha_case("56 bytes — the padding edge",
             (const uint8_t *)"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56,
             "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    sha_case("112 bytes — two blocks plus padding",
             (const uint8_t *)"abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
                              "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu", 112,
             "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");

    /* One large input, so a bug that only shows after many compressions cannot
       hide behind short test strings. */
    {
        uint8_t *big = malloc(1000000);
        if (!big) { puts("   (skipped 1e6 case: out of memory)"); }
        else {
            memset(big, 'a', 1000000);
            sha_case("one million 'a'", big, 1000000,
                     "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
            free(big);
        }
    }

    puts("");
    puts("== HMAC-SHA-256 against RFC 4231 ==");

    for (i = 0; i < 20; i++) buf[i] = 0x0b;
    hmac_case("case 1 — 20-byte key", buf, 20, (const uint8_t *)"Hi There", 8,
              "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");

    hmac_case("case 2 — short ASCII key", (const uint8_t *)"Jefe", 4,
              (const uint8_t *)"what do ya want for nothing?", 28,
              "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");

    {
        uint8_t k[20], m[50];
        for (i = 0; i < 20; i++) k[i] = 0xaa;
        for (i = 0; i < 50; i++) m[i] = 0xdd;
        hmac_case("case 3 — 20-byte key, 50-byte data", k, 20, m, 50,
                  "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe");
    }

    /* ⚠ CASE 6 IS THE ONE THAT MATTERS MOST HERE. A key longer than the 64-byte
       block must be HASHED first — not truncated, not zero-padded. Our real key
       is short, so this branch is never taken in the field: if it were wrong,
       every other test would stay green and nobody would ever find out until
       someone rotated to a long key and every board silently went unsigned. */
    for (i = 0; i < 131; i++) buf[i] = 0xaa;
    hmac_case("case 6 — key longer than the block", buf, 131,
              (const uint8_t *)"Test Using Larger Than Block-Size Key - Hash Key First", 54,
              "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");

    /* ⚠ THE COPY/HASH BOUNDARY: 64 bytes is copied, 65 is hashed. An off-by-one
       in that comparison is invisible to every RFC case above, because none of
       them lands on the block size exactly.

       ⚠ AND THESE FOUR EXPECTED VALUES ARE NOT FROM RFC 4231 — no published
       vector covers them. They were computed with Python's hmac/hashlib, which
       is an independent implementation in a different language, so it is still
       an outside oracle rather than this file agreeing with itself. Said out
       loud because "test vector" and "number I generated" look identical once
       pasted, and only one of them is evidence. */
    {
        uint8_t k[65];
        for (i = 0; i < 65; i++) k[i] = 0xaa;
        hmac_case("64-byte key — copied, not hashed", k, 64,
                  (const uint8_t *)"boundary", 8,
                  "75eb5ffe3a1f602eab7e09004e78064769aa0eed261e4a3888dfe62d6a945b4e");
        hmac_case("65-byte key — hashed, not truncated", k, 65,
                  (const uint8_t *)"boundary", 8,
                  "8667c9376a80b4946a91a671f539eb3769a0928f3ccbf5c819a0b5af61f86d10");
    }

    /* Degenerate inputs. An empty key is what a board built from
       secrets.h.example without filling it in would produce — it must compute
       something deterministic rather than read past the buffer. */
    hmac_case("empty key", (const uint8_t *)"", 0, (const uint8_t *)"boundary", 8,
              "2c6150df91d94a25a8bf27130093b7a25b6ac1238c7092be2efffc3040248e73");
    hmac_case("empty message", (const uint8_t *)"key", 3, (const uint8_t *)"", 0,
              "5d5d139563c95b5967b9bd9a8c9b233a9dedb45072794cd232dc1b74832607d0");

    puts("");
    puts("== the refusal ==");
    {
        char small[10];
        if (pb_hmac_hex((const uint8_t *)"k", 1, (const uint8_t *)"m", 1, small, sizeof small) != 0) {
            puts("   \033[31mFAIL\033[0m a too-small buffer was written to anyway");
            failures++;
        } else {
            puts("   \033[32mok\033[0m   refuses a buffer that cannot hold the digest");
        }
    }

    puts("");
    if (failures) {
        printf("\033[31m%d FAILURE(S)\033[0m\n", failures);
        return 1;
    }
    puts("\033[32mall vectors matched\033[0m");
    return 0;
}

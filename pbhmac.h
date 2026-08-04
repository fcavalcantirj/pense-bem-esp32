#pragma once
/* pbhmac.h — HMAC-SHA256, by hand, in pure C99.
 *
 * ⚠ WHY THIS EXISTS AT ALL, AND WHAT IT DOES NOT BUY.
 *
 * The board signs its one message so the counter can tell "a build that knows
 * the key" from "anything that can reach port 80". That is the whole job.
 *
 * It is NOT a lock and describing it as one would be the first lie. This
 * firmware is public so strangers can build boards, and the counter's gate
 * counts THEIR boards — so the key has to be obtainable by a stranger, which
 * means it is obtainable by anyone. There is no design that escapes that, and
 * there is no hardware escape on this board either: the only device-unique
 * value on a classic ESP32 is the factory eFuse base MAC, which is broadcast in
 * every WiFi frame and so identifies without authenticating. (The eFuse-backed
 * HMAC peripheral that would give a real root of trust does not exist on this
 * silicon — it starts at the S2/S3/C3.)
 *
 * What it buys is a cost increase: forging a signed board goes from one curl to
 * reading this repo and implementing HMAC. Worth it, and worth saying plainly.
 *
 * ⚠ WHY NOT mbedtls, WHICH IS ALREADY LINKED. Two reasons, and the second is
 * the real one. It would tie this file to an Arduino/ESP-IDF header, so the
 * host suite could no longer compile the SAME code the board runs — and this
 * repo's whole testing posture is one copy, no mirror, no drift. And the flash
 * partition is near full; ~3 KB of C is cheaper than pulling in a md_context.
 *
 * ⚠ uint32_t EVERYWHERE, NEVER `unsigned long`. That is a scar, not a style
 * preference: `unsigned long` is 64-bit on the Mac and 32-bit on the ESP32, and
 * this project has already shipped a host suite that was green while the device
 * computed something different. SHA-256 is defined on exact 32-bit words; a
 * type that changes width between host and target silently changes the answer.
 *
 * Verified against the published RFC 4231 test vectors in hello-test/, the same
 * four the Go server is checked against — two independent implementations
 * agreeing with a third-party constant is evidence. Agreeing with each other
 * would only be a coincidence.
 */

#include <stddef.h>
#include <stdint.h>

#define PB_SHA256_DIGEST 32
#define PB_SHA256_BLOCK  64
/* 64 hex characters plus a terminator. */
#define PB_HMAC_HEX_LEN  65

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t  buf[PB_SHA256_BLOCK];
    size_t   buflen;
} pb_sha256_ctx;

static const uint32_t pb_sha256_k[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

static inline uint32_t pb_rotr(uint32_t x, unsigned n)
{
    /* & 31u so a rotate of 32 is not undefined; every call site here is 1..25,
       but a shift equal to the width is UB and costs nothing to exclude. */
    return (x >> (n & 31u)) | (x << ((32u - n) & 31u));
}

static inline void pb_sha256_compress(pb_sha256_ctx *c, const uint8_t *p)
{
    uint32_t w[64], a, b, d, e, f, g, h, cc, t1, t2;
    int i;

    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | ((uint32_t)p[i * 4 + 3]);
    }
    for (i = 16; i < 64; i++) {
        uint32_t s0 = pb_rotr(w[i - 15], 7) ^ pb_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = pb_rotr(w[i - 2], 17) ^ pb_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    a = c->state[0]; b = c->state[1]; cc = c->state[2]; d = c->state[3];
    e = c->state[4]; f = c->state[5]; g  = c->state[6]; h = c->state[7];

    for (i = 0; i < 64; i++) {
        uint32_t S1 = pb_rotr(e, 6) ^ pb_rotr(e, 11) ^ pb_rotr(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t S0 = pb_rotr(a, 2) ^ pb_rotr(a, 13) ^ pb_rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        t1 = h + S1 + ch + pb_sha256_k[i] + w[i];
        t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = cc; cc = b; b = a; a = t1 + t2;
    }

    c->state[0] += a; c->state[1] += b; c->state[2] += cc; c->state[3] += d;
    c->state[4] += e; c->state[5] += f; c->state[6] += g;  c->state[7] += h;
}

static inline void pb_sha256_init(pb_sha256_ctx *c)
{
    c->state[0] = 0x6a09e667u; c->state[1] = 0xbb67ae85u;
    c->state[2] = 0x3c6ef372u; c->state[3] = 0xa54ff53au;
    c->state[4] = 0x510e527fu; c->state[5] = 0x9b05688cu;
    c->state[6] = 0x1f83d9abu; c->state[7] = 0x5be0cd19u;
    c->bitlen = 0;
    c->buflen = 0;
}

static inline void pb_sha256_update(pb_sha256_ctx *c, const uint8_t *data, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++) {
        c->buf[c->buflen++] = data[i];
        if (c->buflen == PB_SHA256_BLOCK) {
            pb_sha256_compress(c, c->buf);
            c->bitlen += (uint64_t)PB_SHA256_BLOCK * 8u;
            c->buflen = 0;
        }
    }
}

static inline void pb_sha256_final(pb_sha256_ctx *c, uint8_t out[PB_SHA256_DIGEST])
{
    size_t i = c->buflen;
    uint64_t bits;

    /* Pad: 0x80, zeroes, then the 64-bit big-endian length. If the length no
       longer fits in this block, flush and pad a second one. */
    c->buf[i++] = 0x80u;
    if (i > 56) {
        while (i < PB_SHA256_BLOCK) c->buf[i++] = 0x00u;
        pb_sha256_compress(c, c->buf);
        i = 0;
    }
    while (i < 56) c->buf[i++] = 0x00u;

    bits = c->bitlen + (uint64_t)c->buflen * 8u;
    for (i = 0; i < 8; i++) {
        c->buf[63 - i] = (uint8_t)((bits >> (8u * (unsigned)i)) & 0xFFu);
    }
    pb_sha256_compress(c, c->buf);

    for (i = 0; i < 8; i++) {
        out[i * 4]     = (uint8_t)((c->state[i] >> 24) & 0xFFu);
        out[i * 4 + 1] = (uint8_t)((c->state[i] >> 16) & 0xFFu);
        out[i * 4 + 2] = (uint8_t)((c->state[i] >> 8) & 0xFFu);
        out[i * 4 + 3] = (uint8_t)(c->state[i] & 0xFFu);
    }
}

/* pb_hmac_sha256 — RFC 2104, out must have room for PB_SHA256_DIGEST bytes.
 *
 * ⚠ A KEY LONGER THAN THE BLOCK IS HASHED, NOT TRUNCATED. This is the branch
 * hand-rolled HMACs get wrong, and nothing in normal operation exercises it
 * because our key is short — so RFC 4231 case 6 (a 131-byte key) is asserted by
 * name in the host suite. A control that fires nowhere is the one nobody audits.
 */
static inline void pb_hmac_sha256(const uint8_t *key, size_t keylen,
                                  const uint8_t *msg, size_t msglen,
                                  uint8_t out[PB_SHA256_DIGEST])
{
    uint8_t k[PB_SHA256_BLOCK];
    uint8_t inner[PB_SHA256_DIGEST];
    uint8_t pad[PB_SHA256_BLOCK];
    pb_sha256_ctx c;
    size_t i;

    for (i = 0; i < PB_SHA256_BLOCK; i++) k[i] = 0x00u;
    if (keylen > PB_SHA256_BLOCK) {
        pb_sha256_init(&c);
        pb_sha256_update(&c, key, keylen);
        pb_sha256_final(&c, k);       /* leaves bytes 32..63 zero */
    } else {
        for (i = 0; i < keylen; i++) k[i] = key[i];
    }

    for (i = 0; i < PB_SHA256_BLOCK; i++) pad[i] = (uint8_t)(k[i] ^ 0x36u);
    pb_sha256_init(&c);
    pb_sha256_update(&c, pad, PB_SHA256_BLOCK);
    pb_sha256_update(&c, msg, msglen);
    pb_sha256_final(&c, inner);

    for (i = 0; i < PB_SHA256_BLOCK; i++) pad[i] = (uint8_t)(k[i] ^ 0x5cu);
    pb_sha256_init(&c);
    pb_sha256_update(&c, pad, PB_SHA256_BLOCK);
    pb_sha256_update(&c, inner, PB_SHA256_DIGEST);
    pb_sha256_final(&c, out);
}

/* pb_hmac_hex writes the lowercase hex digest plus a terminator.
 *
 * ⚠ LOWERCASE, and the server compares the header as TEXT rather than decoding
 * it — so uppercase would be rejected. Both ends agreeing on one spelling is
 * cheaper than either end normalising, because normalising is the kind of
 * change that gets made on one side only.
 *
 * outCap must be at least PB_HMAC_HEX_LEN. Returns the length written, or 0 if
 * it would not fit — a truncated signature is not a weaker signature, it is a
 * request that will simply be marked unsigned. */
static inline size_t pb_hmac_hex(const uint8_t *key, size_t keylen,
                                 const uint8_t *msg, size_t msglen,
                                 char *out, size_t outCap)
{
    static const char hex[] = "0123456789abcdef";
    uint8_t d[PB_SHA256_DIGEST];
    size_t i;

    if (outCap < PB_HMAC_HEX_LEN) return 0;
    pb_hmac_sha256(key, keylen, msg, msglen, d);
    for (i = 0; i < PB_SHA256_DIGEST; i++) {
        out[i * 2]     = hex[(d[i] >> 4) & 0x0Fu];
        out[i * 2 + 1] = hex[d[i] & 0x0Fu];
    }
    out[PB_SHA256_DIGEST * 2] = '\0';
    return PB_SHA256_DIGEST * 2;
}

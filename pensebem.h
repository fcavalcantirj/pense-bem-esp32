/* pensebem.h — the Pense Bem answer formula, and NOTHING else.
 *
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <leandro@tia.mat.br> and <juca@members.fsf.org> wrote this file. As long as
 * you retain this notice you can do whatever you want with this stuff.  If we
 * meet some day, and you think this stuff is worth it, you can buy us a beer
 * in return.
 * ----------------------------------------------------------------------------
 *
 * That notice is retained VERBATIM and not paraphrased, because "this stuff" is
 * precisely pb_seed_book_one[] and pb_chained_offsets[] below — those two
 * constants ARE the reverse-engineering work the licence covers. A credit line
 * that says "derived from Beerware-licensed work" is not the notice.
 * Upstream: github.com/lpereira/Pense-Bem and github.com/ehabkost/pensebem.
 *
 *
 * WHAT THIS IS
 * ------------
 * The 1988 Tectoy "Pense Bem" ran on a Zilog Z8 with 128 bytes of RAM and 2 KB
 * of ROM. There was no room to store 14 850 answers, so the toy DERIVES the
 * correct answer from (book, question) arithmetically. Every printed activity
 * book was authored to match what the device already computes — which is why one
 * formula validates every book ever printed, and why a "brand" (Sonic, Turma da
 * Monica, Thor) is purely cosmetic.
 *
 *
 * ⚠⚠ THIS FILE IS COMPILED BY TWO TOOLCHAINS AND MUST STAY THAT WAY:
 *       xtensa-esp32-elf-g++   (via esp32-pense-bem.ino, C++)
 *       cc -std=c99 -Wall -Wextra -Werror  (via formula-test/pb-formula.c, C99)
 *
 *   That dual compile is the ONLY reason formula-test proves anything about the
 *   SHIPPED code rather than about a copy of it. This repo's first firmware test
 *   (a sibling project) is a MIRROR — it re-types the
 *   sketch's expressions and leans on a grep drift guard to stay honest. This
 *   file exists so that never has to happen again: the sketch and the test
 *   #include the same bytes, so there is nothing to drift.
 *
 *   So: NO <Arduino.h>. NO String. NO Serial. NO millis. NO pinMode. NO malloc.
 *       NO <stdbool.h> (use int). NO C++-only syntax. NO floating point.
 *   formula-test/run.sh stage 1 greps for every one of those and fails loudly.
 *
 *   ⚠ And it greps that the seed constant appears in THIS FILE AND NOWHERE ELSE
 *     in the device folder. The moment anyone pastes these constants into the
 *     .ino, "the test reads the shipped code" becomes a lie, and that grep is
 *     the only thing that would catch it.
 */
#ifndef PENSEBEM_H
#define PENSEBEM_H

/* C89-compatible compile-time assert: _Static_assert is C11-only and
   static_assert is C++-only, and this header must satisfy both compilers. */
#define PB_STATIC_ASSERT(cond, tag) typedef char pb_sa_##tag[(cond) ? 1 : -1]

#define PB_BOOKS           99
#define PB_QUESTIONS      150
#define PB_PER_ROUND       30   /* questions per section, sections 1-5          */
#define PB_PAGE             5   /* questions per printed page (section 6 rule)  */
#define PB_SECTIONS         6
#define PB_SHIFT_Q         14   /* q % 30 == 14 -> consume an offset            */
#define PB_POPS_PER_BOOK    6   /* five mid-round pops + the q==149 wrap        */
#define PB_OFFSETS        588

/* 588 = 98 books x 6 pops, exactly. Measured [REAL] 2026-08-02: the queue is
   consumed to zero with none left over. Pinning it at compile time makes that
   fact unfalsifiable by a typo in either constant. */
PB_STATIC_ASSERT(PB_OFFSETS == (PB_BOOKS - 1) * PB_POPS_PER_BOOK, offset_budget);
PB_STATIC_ASSERT(PB_QUESTIONS == PB_PER_ROUND * (PB_SECTIONS - 1), section_span);
PB_STATIC_ASSERT(PB_QUESTIONS / PB_PAGE == PB_PER_ROUND, review_pages);

/* Book 1's 150 answers, verbatim. Every other book is derived from this. */
static const char pb_seed_book_one[PB_QUESTIONS + 1] =
    "dbaadcbdaadcbbc"
    "bdddbdababdacac"
    "cbdababdacaccbd"
    "bbcdcdddacaabca"
    "abadbbbcdcdddac"
    "cbadbadbbddccba"
    "dbadbbdcccbadba"
    "bbdabbdabdabccd"
    "dccddaacdbbddbb"
    "cdcbbbdabdddcdc";

/* The base-4 offset queue, consumed strictly in order across all 98 derivations. */
static const char pb_chained_offsets[PB_OFFSETS + 1] =
    "22221202301023123110332032313302"
    "03022121320233203323333220221221"
    "30303330010113102300312222030031"
    "22201303322312111332102302332023"
    "12033033201101201022100330112212"
    "31101032132131211313212111330313"
    "23120203032010023131303302312120"
    "03233301131332001130130102322321"
    "00101020113320201200223033300200"
    "20332303233320232301303322112030"
    "33000131223323032222211303211222"
    "01022012130321201023122111120300"
    "31213021320123211301301322230130"
    "22030130333312012220221103001133"
    "10031131131230212010110223103300"
    "32322123132020333001212020032303"
    "10302221221023033011310303012012"
    "12012031321213323020123321303210"
    "013020120331";

PB_STATIC_ASSERT(sizeof(pb_seed_book_one)   == PB_QUESTIONS + 1, seed_len);
PB_STATIC_ASSERT(sizeof(pb_chained_offsets) == PB_OFFSETS + 1,   offsets_len);

/* ---------------------------------------------------------------- state ---- */

static unsigned char pb_table[PB_BOOKS][PB_QUESTIONS];
static int pb_popped;
static int pb_built;

/* ------------------------------------------------------------ generator ---- */

/* Builds all 99 x 150 answers. Idempotent; call once.
 *
 * WHY THE WHOLE TABLE AND NOT ONE BOOK AT A TIME:
 * 14 850 bytes in .bss is 4.6 % of this chip's DRAM, and the build is a single
 * pass of ~14 700 byte operations — well under a millisecond. Generating one
 * book on demand into 150 bytes would be just as fast (books share one offset
 * queue consumed in order, so "generate book N" IS "generate 1..N and discard"),
 * so this is not a performance argument. It is that a 150-byte cache introduces
 * an invalidation invariant on the one function whose wrong output is
 * UNFALSIFIABLE: a toy saying CERTO to the wrong letter looks exactly like a toy
 * saying CERTO to the right letter. No relay, no log, no downstream consumer, no
 * second opinion. A stateless O(1) lookup cannot desynchronise — the defect is
 * unrepresentable rather than merely untriggered.
 */
static inline void pb_init(void)
{
    unsigned char pat[PB_QUESTIONS];
    int book, q, i;

    if (pb_built) {
        return;
    }
    for (i = 0; i < PB_QUESTIONS; i++) {
        pat[i] = (unsigned char)pb_seed_book_one[i];
    }
    pb_popped = 0;

    for (book = 0; book < PB_BOOKS; book++) {
        if (book > 0) {
            /* ⚠⚠ CAPTURED HERE, BEFORE THE LOOP, AND THAT IS THE WHOLE TRAP.
               pat[0] is overwritten on the very first iteration below, so
               reading pat[0] at q==149 would read THIS book's fresh value
               instead of the previous book's.

               Measured [REAL] 2026-08-02: that mutation corrupts 3681 of 14850
               cells — but NOT ONE of them is a question below #53, so 903 of the
               925 distinct real-book observations stay GREEN and only books
               2, 47 and 92 catch it. formula-test/run.sh names those three. */
            unsigned char first = pat[0];

            for (q = 0; q < PB_QUESTIONS; q++) {
                unsigned char shift = 0;
                unsigned char prev;

                if (q % PB_PER_ROUND == PB_SHIFT_Q || q == PB_QUESTIONS - 1) {
                    shift = (unsigned char)(pb_chained_offsets[pb_popped] - '0');
                    pb_popped++;
                }
                prev = (q == PB_QUESTIONS - 1) ? first : pat[q + 1];
                pat[q] = (unsigned char)('a' + ((prev - 'a' + shift) % 4));
            }
        }
        for (q = 0; q < PB_QUESTIONS; q++) {
            pb_table[book][q] = (unsigned char)(pat[q] - 'a' + 'A');
        }
    }
    pb_built = 1;
}

/* Audit counter. MUST read exactly PB_OFFSETS after pb_init(). Not >=, not <=:
   a mutation that drops the q==149 pop consumes 490 and would otherwise pass a
   "did we consume some offsets" check. */
static inline int pb_offsets_popped(void)
{
    return pb_popped;
}

/* -------------------------------------------------------------- answers ---- */

/* Upstream stores answers lowercase; this header speaks uppercase. Exposed so
   consumers never hand-roll `c - 'a' + 'A'` — which lets formula-test keep a
   STRICT ban on char arithmetic in its driver, so the driver cannot quietly
   grow into a second implementation of the chain. */
static inline char pb_upper(char c)
{
    return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
}

/* 'A'..'D' for a 1-based book and question; 0 on bad input.
 *
 * ⚠ THIS ACCEPTS BOOKS 17, 18 AND 19 ON PURPOSE. The reference Go port rejects
 *   them (answers.go:137) reading upstream FINDINGS' "Books # 019,018,017:
 *   rejected" as book numbers. Under FINDINGS' own BBS diagram those are book
 *   01, sections 7/8/9 — invalid SECTIONS, already excluded by the section rule.
 *   FINDINGS writes wildcards as "00?" and "??0"; those three are literal codes,
 *   exactly like its other examples (#011, #012, #015, #016 — all book 01).
 *   The harvested fixture settles it: codes 171 and 181 carry 30 real observed
 *   answers each and 191 carries 1, while 017/018/019 carry none.
 *   Book restrictions are a UI rule and live in pb_code_valid(). This is
 *   arithmetic and has none. */
static inline char pb_answer(int book, int question)
{
    if (book < 1 || book > PB_BOOKS) {
        return 0;
    }
    if (question < 1 || question > PB_QUESTIONS) {
        return 0;
    }
    return (char)pb_table[book - 1][question - 1];
}

/* All 150 answers for a book as 'A'..'D'; 0 on bad input. Not NUL-terminated. */
static inline const unsigned char *pb_answer_key(int book)
{
    if (book < 1 || book > PB_BOOKS) {
        return 0;
    }
    return pb_table[book - 1];
}

/* --------------------------------------------------------- activity code --- */

/* The original 3-digit code is BBS: two book digits + one section digit.
   Invalid: book 00 and section 0 (upstream FINDINGS: "Invalid book numbers:
   00?, ??0"). Nothing else — see the note on pb_answer(). */
static inline int pb_code_valid(int book, int section)
{
    return book >= 1 && book <= PB_BOOKS && section >= 1 && section <= PB_SECTIONS;
}

/* Parses "011" style codes. Returns 1 and fills book/section, or 0. */
static inline int pb_parse_code(const char *three, int *book, int *section)
{
    int b, s, i;

    if (three == 0) {
        return 0;
    }
    for (i = 0; i < 3; i++) {
        if (three[i] < '0' || three[i] > '9') {
            return 0;
        }
    }
    if (three[3] != '\0') {
        return 0;
    }
    b = (three[0] - '0') * 10 + (three[1] - '0');
    s = three[2] - '0';
    if (!pb_code_valid(b, s)) {
        return 0;
    }
    if (book) {
        *book = b;
    }
    if (section) {
        *section = s;
    }
    return 1;
}

/* ------------------------------------------------------ question ordering -- */

/* Sections 1-5 are fixed 30-question blocks: 1-30, 31-60, 61-90, 91-120,
   121-150. Index is 0..29. Returns the absolute question number, or 0. */
static inline int pb_section_question(int section, int index)
{
    if (section < 1 || section > PB_SECTIONS - 1) {
        return 0;
    }
    if (index < 0 || index >= PB_PER_ROUND) {
        return 0;
    }
    return (section - 1) * PB_PER_ROUND + index + 1;
}

/* Section 6 is the review round: ONE question from each of the 30 five-question
   pages, in page order.
 *
 * Upstream FINDINGS guessed this ("It looks like it gets a random question from
 * each page, being 5 questions per page"). Confirmed empirically [REAL]
 * 2026-08-02 against the harvested fixture: codes 476 and 926 are complete
 * captures and each holds exactly 30 questions on 30 distinct pages, ascending.
 *
 * Same LCG constants as the Go port so both implementations agree given a seed.
 *
 * ⚠ THE STATE IS `unsigned long long`, NOT `unsigned long`, AND THAT IS LOAD-BEARING.
 *   `unsigned long` is 64-bit on this Mac and 32-bit on the ESP32. The multiplier
 *   6364136223846793005 does not fit in 32 bits and `state >> 33` is undefined on
 *   a 32-bit type — so with `unsigned long` the host test would pass while the
 *   device silently generated a DIFFERENT review round. A host harness that
 *   cannot see the target's word size is exactly the kind of green that means
 *   nothing. `unsigned long long` is >= 64 bits in both C99 and C++11. */
static inline int pb_review_questions(unsigned long long seed, int out[PB_PER_ROUND])
{
    unsigned long long state = seed;
    int page;

    if (out == 0) {
        return 0;
    }
    if (state == 0) {
        state = 0x9e3779b97f4a7c15ULL;
    }
    for (page = 0; page < PB_QUESTIONS / PB_PAGE; page++) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        out[page] = page * PB_PAGE + (int)((state >> 33) % (unsigned long long)PB_PAGE) + 1;
    }
    return PB_QUESTIONS / PB_PAGE;
}

/* Fills out[] for any section. Returns the count, or 0 on bad input. */
static inline int pb_questions_for(int section, unsigned long long seed, int out[PB_PER_ROUND])
{
    int i;

    if (section == PB_SECTIONS) {
        return pb_review_questions(seed, out);
    }
    if (section < 1 || section > PB_SECTIONS - 1 || out == 0) {
        return 0;
    }
    for (i = 0; i < PB_PER_ROUND; i++) {
        out[i] = pb_section_question(section, i);
    }
    return PB_PER_ROUND;
}

/* -------------------------------------------------------------- scoring ---- */

/* Upstream simulator's pointsByNumberOfTries: 10 / 6 / 4 for the 1st / 2nd /
   3rd try, 0 after that. A perfect 30-question round is 300 raw points. */
static inline int pb_points_for_attempt(int attempt)
{
    switch (attempt) {
    case 1:  return 10;
    case 2:  return 6;
    case 3:  return 4;
    default: return 0;
    }
}

/* ⚠ NORMALISE BEFORE BANDING. The four end-of-session songs are defined by
   upstream FINDINGS on a 0-100 scale (76-100, 51-75, 26-50, 0-25). Feeding a raw
   0-300 score straight into those bands makes two of the four songs unreachable,
   so the toy would only ever play half its music. */
static inline int pb_normalized(int raw, int n_questions)
{
    if (n_questions <= 0) {
        return 0;
    }
    if (raw < 0) {
        raw = 0;
    }
    return (raw * 100) / (n_questions * 10);
}

/* 0 = 76-100, 1 = 51-75, 2 = 26-50, 3 = 0-25. Matches FINDINGS' song order. */
static inline int pb_band(int normalized)
{
    if (normalized >= 76) {
        return 0;
    }
    if (normalized >= 51) {
        return 1;
    }
    if (normalized >= 26) {
        return 2;
    }
    return 3;
}

#endif /* PENSEBEM_H */

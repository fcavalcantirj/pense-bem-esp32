/* pb-formula.c — host driver for the Pense Bem formula.
 *
 * ⚠ THIS FILE OWNS NO ALGORITHM. It #includes ../pensebem.h — the same bytes the
 *   sketch compiles — loads fixtures, compares, and prints machine-readable
 *   counts. If any arithmetic ever appears here, the suite stops testing the
 *   shipped code and starts testing a copy of it, which is the exact failure
 *   a sibling project confesses to. run.sh stage 1
 *   greps this file to keep that true.
 *
 * Usage:  pb-formula props | books <tsv> | table <blob> | budget <tsv> | key <book>
 * Exit:   0 = all assertions held, 1 = a failure, 2 = usage/IO error.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../pensebem.h"

static int failures;

#define CHECK(cond, ...)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("  FAIL " __VA_ARGS__);                                     \
            printf("\n");                                                      \
            failures++;                                                        \
        }                                                                      \
    } while (0)

/* ------------------------------------------------------------- fixtures --- */

#define MAX_ROWS 4096

struct Row {
    int book;
    int section;
    int question;
    char answer; /* 'A'..'D' */
};

static struct Row rows[MAX_ROWS];
static int n_rows;

/* Reads the harvested tab-separated fixture: "book<TAB>question<TAB>answer",
   one header line, blank lines allowed. Codes are BBS (011 = book 01, sec 1). */
static int load_books(const char *path)
{
    char line[256];
    FILE *f = fopen(path, "r");
    int first = 1;

    if (!f) {
        fprintf(stderr, "cannot open %s\n", path);
        return 0;
    }
    n_rows = 0;
    while (fgets(line, sizeof(line), f)) {
        int code, q;
        char a[8];

        if (first) {
            first = 0;
            continue;
        }
        if (sscanf(line, "%d %d %7s", &code, &q, a) != 3) {
            continue; /* blank line */
        }
        if (a[0] < 'a' || a[0] > 'd' || a[1] != '\0') {
            continue;
        }
        if (n_rows >= MAX_ROWS) {
            fprintf(stderr, "fixture larger than MAX_ROWS\n");
            fclose(f);
            return 0;
        }
        rows[n_rows].book = code / 10;
        rows[n_rows].section = code % 10;
        rows[n_rows].question = q;
        rows[n_rows].answer = pb_upper(a[0]);
        n_rows++;
    }
    fclose(f);
    return n_rows > 0;
}

static char *load_blob(const char *path, long *len)
{
    char *buf;
    long n;
    FILE *f = fopen(path, "rb");

    if (!f) {
        fprintf(stderr, "cannot open %s\n", path);
        return 0;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (char *)malloc((size_t)n + 1);
    if (!buf) {
        fclose(f);
        return 0;
    }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        fclose(f);
        return 0;
    }
    fclose(f);
    buf[n] = '\0';
    *len = n;
    return buf;
}

/* ------------------------------------------------------------ properties --- */

static int mode_props(void)
{
    int book, q, s, i, seed;
    int seen[PB_QUESTIONS + 1];
    int qs[PB_PER_ROUND];
    int valid = 0;
    int bands[4];

    /* Exactly 588. Not >=, not <=: dropping the q==149 pop consumes 490 and
       would sail through any "did we consume some" check. */
    CHECK(pb_offsets_popped() == PB_OFFSETS,
          "offsets popped = %d, want exactly %d", pb_offsets_popped(), PB_OFFSETS);

    /* Every cell is a legal answer. */
    for (book = 1; book <= PB_BOOKS; book++) {
        for (q = 1; q <= PB_QUESTIONS; q++) {
            char a = pb_answer(book, q);
            if (a < 'A' || a > 'D') {
                CHECK(0, "book %d q %d = 0x%02x, not A-D", book, q, (unsigned)a);
                book = PB_BOOKS + 1;
                break;
            }
        }
    }

    /* Book 1 is the seed, uppercased, byte for byte. */
    for (q = 0; q < PB_QUESTIONS; q++) {
        CHECK(pb_answer(1, q + 1) == pb_upper(pb_seed_book_one[q]),
              "book 1 q %d diverges from the seed constant", q + 1);
    }

    /* Out-of-range is rejected, not clamped. */
    CHECK(pb_answer(0, 1) == 0, "book 0 should be rejected");
    CHECK(pb_answer(PB_BOOKS + 1, 1) == 0, "book 100 should be rejected");
    CHECK(pb_answer(1, 0) == 0, "question 0 should be rejected");
    CHECK(pb_answer(1, PB_QUESTIONS + 1) == 0, "question 151 should be rejected");
    CHECK(pb_answer_key(1) != 0, "answer key for book 1 should exist");
    CHECK(pb_answer_key(0) == 0, "answer key for book 0 should be null");

    /* Code validity swept over every (book, section) pair. Only book 00 and
       section 0 are invalid -> 99 books x 6 sections. */
    for (book = 0; book <= 99; book++) {
        for (s = 0; s <= 9; s++) {
            if (pb_code_valid(book, s)) {
                valid++;
            }
        }
    }
    CHECK(valid == PB_BOOKS * PB_SECTIONS,
          "valid codes = %d, want %d (99 books x 6 sections)",
          valid, PB_BOOKS * PB_SECTIONS);

    /* Books 17/18/19 are REAL. The Go port rejects them; the fixture holds 61
       observed answers for them. Pinned so nobody "cleans this up" back. */
    CHECK(pb_code_valid(17, 1), "book 17 must be valid (fixture has 30 rows for 171)");
    CHECK(pb_code_valid(18, 1), "book 18 must be valid (fixture has 30 rows for 181)");
    CHECK(pb_code_valid(19, 1), "book 19 must be valid (fixture has 1 row for 191)");
    CHECK(pb_parse_code("171", 0, 0), "code 171 must parse");
    CHECK(pb_parse_code("186", 0, 0), "code 186 must parse");
    CHECK(!pb_parse_code("017", 0, 0), "code 017 is section 7 -> must be rejected");
    CHECK(!pb_parse_code("001", 0, 0), "book 00 must be rejected");
    CHECK(!pb_parse_code("010", 0, 0), "section 0 must be rejected");
    CHECK(!pb_parse_code("1A1", 0, 0), "non-digits must be rejected");
    CHECK(!pb_parse_code("1111", 0, 0), "4 digits must be rejected");
    CHECK(!pb_parse_code("11", 0, 0), "2 digits must be rejected");

    /* Sections 1-5 tile 1..150 with no gaps and no repeats.
       ⚠ Section 2 is asserted BY NAME. At section 1 the (s-1)*30 term is zero,
       so a wrong questions-per-section constant is undetectable there — the
       shared-term degeneracy is worth naming explicitly. */
    CHECK(pb_section_question(2, 0) == 31, "section 2 must start at question 31");
    CHECK(pb_section_question(5, 29) == 150, "section 5 must end at question 150");
    CHECK(pb_section_question(1, 0) == 1, "section 1 must start at question 1");
    CHECK(pb_section_question(6, 0) == 0, "section 6 is review, not a fixed block");
    CHECK(pb_section_question(1, PB_PER_ROUND) == 0, "index 30 must be rejected");
    memset(seen, 0, sizeof(seen));
    for (s = 1; s <= 5; s++) {
        for (i = 0; i < PB_PER_ROUND; i++) {
            int qq = pb_section_question(s, i);
            CHECK(qq >= 1 && qq <= PB_QUESTIONS, "section %d idx %d out of range", s, i);
            if (qq >= 1 && qq <= PB_QUESTIONS) {
                seen[qq]++;
            }
        }
    }
    for (q = 1; q <= PB_QUESTIONS; q++) {
        CHECK(seen[q] == 1, "question %d covered %d times by sections 1-5", q, seen[q]);
    }

    /* Section 6: exactly one question per 5-question page, in page order.
       Swept over 1000 seeds so a lucky single seed cannot carry it. */
    for (seed = 1; seed <= 1000; seed++) {
        int n = pb_questions_for(PB_SECTIONS, (unsigned long long)seed * 2654435761ULL, qs);
        if (n != PB_PER_ROUND) {
            CHECK(0, "seed %d produced %d review questions, want %d", seed, n, PB_PER_ROUND);
            break;
        }
        for (i = 0; i < PB_PER_ROUND; i++) {
            int lo = i * PB_PAGE + 1;
            int hi = lo + PB_PAGE - 1;
            if (qs[i] < lo || qs[i] > hi) {
                CHECK(0, "seed %d page %d gave q %d, want %d..%d", seed, i, qs[i], lo, hi);
                seed = 1001;
                break;
            }
            if (i > 0 && qs[i] <= qs[i - 1]) {
                CHECK(0, "seed %d not ascending at page %d", seed, i);
                seed = 1001;
                break;
            }
        }
    }

    /* Scoring. ⚠ All four bands must be reachable across the 31 possible
       all-first-try scores. Banding a raw 0-300 score directly makes two of the
       four songs unreachable, and the toy would only ever play half its music. */
    CHECK(pb_points_for_attempt(1) == 10, "first try must score 10");
    CHECK(pb_points_for_attempt(2) == 6, "second try must score 6");
    CHECK(pb_points_for_attempt(3) == 4, "third try must score 4");
    CHECK(pb_points_for_attempt(4) == 0, "fourth try must score 0");
    CHECK(pb_normalized(300, 30) == 100, "a perfect round must normalise to 100");
    CHECK(pb_normalized(0, 30) == 0, "zero must normalise to 0");
    memset(bands, 0, sizeof(bands));
    for (i = 0; i <= 30; i++) {
        bands[pb_band(pb_normalized(i * 10, 30))]++;
    }
    for (i = 0; i < 4; i++) {
        CHECK(bands[i] > 0, "band %d unreachable across the 31 possible scores", i);
    }
    /* Boundaries pinned by name, not by "it's in range". */
    CHECK(pb_band(pb_normalized(23 * 10, 30)) == 0, "23/30 = 76%% must be band 0");
    CHECK(pb_band(pb_normalized(22 * 10, 30)) == 1, "22/30 = 73%% must be band 1");
    CHECK(pb_band(pb_normalized(16 * 10, 30)) == 1, "16/30 = 53%% must be band 1");
    CHECK(pb_band(pb_normalized(15 * 10, 30)) == 2, "15/30 = 50%% must be band 2");
    CHECK(pb_band(pb_normalized(8 * 10, 30)) == 2, "8/30 = 26%% must be band 2");
    CHECK(pb_band(pb_normalized(7 * 10, 30)) == 3, "7/30 = 23%% must be band 3");

    printf("PROPS_FAILURES=%d\n", failures);
    return failures == 0 ? 0 : 1;
}

/* --------------------------------------------------- vs the real books ----- */

static int mode_books(const char *path, int quiet)
{
    int i, wrong = 0, distinct = 0;
    int bad_books[PB_BOOKS + 1];
    int seen_book[PB_BOOKS + 1];
    /* dedupe (book, question) so a repeated capture is not counted twice */
    static char best[PB_BOOKS + 1][PB_QUESTIONS + 1];
    int bands[6];
    int first = 1;

    if (!load_books(path)) {
        return 2;
    }
    memset(bad_books, 0, sizeof(bad_books));
    memset(seen_book, 0, sizeof(seen_book));
    memset(best, 0, sizeof(best));
    memset(bands, 0, sizeof(bands));

    for (i = 0; i < n_rows; i++) {
        if (rows[i].book < 1 || rows[i].book > PB_BOOKS) {
            continue;
        }
        if (rows[i].question < 1 || rows[i].question > PB_QUESTIONS) {
            continue;
        }
        best[rows[i].book][rows[i].question] = rows[i].answer;
        seen_book[rows[i].book] = 1;
    }
    for (i = 1; i <= PB_BOOKS; i++) {
        int q;
        for (q = 1; q <= PB_QUESTIONS; q++) {
            if (!best[i][q]) {
                continue;
            }
            distinct++;
            bands[(q - 1) / PB_PER_ROUND]++;
            if (pb_answer(i, q) != best[i][q]) {
                wrong++;
                bad_books[i] = 1;
            }
        }
    }

    printf("BOOKS_ROWS=%d\n", n_rows);
    printf("BOOKS_DISTINCT=%d\n", distinct);
    {
        int nb = 0;
        for (i = 1; i <= PB_BOOKS; i++) {
            if (seen_book[i]) {
                nb++;
            }
        }
        printf("BOOKS_WITNESSED=%d\n", nb);
    }
    printf("BOOKS_WRONG=%d\n", wrong);
    printf("BOOKS_WRONG_LIST=");
    for (i = 1; i <= PB_BOOKS; i++) {
        if (bad_books[i]) {
            printf("%s%d", first ? "" : ",", i);
            first = 0;
        }
    }
    printf("\n");
    if (!quiet) {
        printf("  coverage by band  q1-30: %d  q31-60: %d  q61-90: %d  q91-120: %d  q121-150: %d\n",
               bands[0], bands[1], bands[2], bands[3], bands[4]);
    }
    return wrong == 0 ? 0 : 1;
}

/* ------------------------------------------- vs the reference generator ---- */

static int mode_table(const char *path)
{
    long len;
    int i, wrong = 0;
    char *blob = load_blob(path, &len);
    int cells = PB_BOOKS * PB_QUESTIONS;

    if (!blob) {
        return 2;
    }
    /* ⚠ The upstream file is 14852 bytes, not 14850 — two trailing characters
       are unconsumed by every upstream reader. Compare only the first 14850 and
       say so, rather than asserting a length that would fail. */
    printf("TABLE_BYTES=%ld\n", len);
    if (len < cells) {
        printf("TABLE_WRONG=%d\n", cells);
        printf("  FAIL fixture holds %ld bytes, need at least %d\n", len, cells);
        free(blob);
        return 1;
    }
    for (i = 0; i < cells; i++) {
        char want = pb_upper(blob[i]);
        if (pb_answer(i / PB_QUESTIONS + 1, i % PB_QUESTIONS + 1) != want) {
            wrong++;
        }
    }
    printf("TABLE_CELLS=%d\n", cells);
    printf("TABLE_WRONG=%d\n", wrong);
    free(blob);
    return wrong == 0 ? 0 : 1;
}

/* ------------------------------------------------------- vacuity budget ---- */

/* ⚠ Prints what this suite CANNOT see. A suite that hides its blind spots reads
   as complete after a context reset, and that is how "982/982, zero errors"
   became a sentence that is ~40 % vacuous. */
static int mode_budget(const char *path)
{
    int i, q, distinct = 0, book1 = 0, witnessed = 0;
    static char best[PB_BOOKS + 1][PB_QUESTIONS + 1];

    if (!load_books(path)) {
        return 2;
    }
    memset(best, 0, sizeof(best));
    for (i = 0; i < n_rows; i++) {
        if (rows[i].book >= 1 && rows[i].book <= PB_BOOKS &&
            rows[i].question >= 1 && rows[i].question <= PB_QUESTIONS) {
            best[rows[i].book][rows[i].question] = rows[i].answer;
        }
    }
    for (i = 1; i <= PB_BOOKS; i++) {
        int any = 0;
        for (q = 1; q <= PB_QUESTIONS; q++) {
            if (best[i][q]) {
                distinct++;
                any = 1;
                if (i == 1) {
                    book1++;
                }
            }
        }
        if (any) {
            witnessed++;
        }
    }
    printf("  %d distinct real-book observations, %d books witnessed, %d books UNWITNESSED\n",
           distinct, witnessed, PB_BOOKS - witnessed);
    printf("  %d of them (%d%%) are book 1 — which SKIPS the transform entirely and\n",
           book1, distinct ? (book1 * 100) / distinct : 0);
    printf("  therefore proves only that a 150-char constant was typed correctly.\n");
    printf("  shift is zero at %d of %d positions: the modular core runs %d times per book.\n",
           PB_QUESTIONS - PB_POPS_PER_BOOK, PB_QUESTIONS, PB_POPS_PER_BOOK);
    printf("  measured: deleting the WHOLE transform still leaves ~40%% of these rows GREEN.\n");
    printf("  measured: ~half of the %d offset digits are invisible to real-book evidence.\n",
           PB_OFFSETS);
    printf("  the reference table closes the port gap; it CANNOT close the formula gap.\n");
    return 0;
}

/* --------------------------------------------------------- parity dump ---- */

/* Prints one book's 150 answers, for cross-checking against the INDEPENDENT Go
   implementation in the sibling Go implementation:
       GET /api/v1/books/{n}/answers
   Two implementations in two languages agreeing is a real check. A header
   agreeing with itself is not. */
static int mode_key(const char *book_s)
{
    int book = atoi(book_s);
    const unsigned char *k = pb_answer_key(book);
    int q;

    if (!k) {
        fprintf(stderr, "book out of range\n");
        return 2;
    }
    for (q = 0; q < PB_QUESTIONS; q++) {
        putchar(k[q]);
    }
    putchar('\n');
    return 0;
}

/* ------------------------------------------------------------------ main --- */

int main(int argc, char **argv)
{
    pb_init();


    if (argc >= 2 && strcmp(argv[1], "props") == 0) {
        return mode_props();
    }
    if (argc >= 3 && strcmp(argv[1], "books") == 0) {
        return mode_books(argv[2], argc >= 4);
    }
    if (argc >= 3 && strcmp(argv[1], "table") == 0) {
        return mode_table(argv[2]);
    }
    if (argc >= 3 && strcmp(argv[1], "budget") == 0) {
        return mode_budget(argv[2]);
    }
    if (argc >= 3 && strcmp(argv[1], "key") == 0) {
        return mode_key(argv[2]);
    }
    fprintf(stderr, "usage: %s props | books <tsv> | table <blob> | budget <tsv> | key <book>\n", argv[0]);
    return 2;
}

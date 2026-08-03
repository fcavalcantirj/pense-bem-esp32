/* pb-game.c — host tests for ../pbgame.h, the pure session state machine.
 *
 * TDD: this file was written BEFORE pbgame.h existed, and run to see it RED.
 *
 * ⚠ WHY THE GAME RULES ARE A SEPARATE PURE HEADER AND NOT LOGIC IN THE .ino:
 *   scoring, retry counting and question advance are the parts a player would
 *   never notice being wrong — a toy that quietly awards 10 points on a second
 *   try looks exactly like one that awards 6. None of it needs a screen, a
 *   button or a millisecond, so none of it belongs anywhere a test cannot reach.
 *   The .ino is left with input mapping and drawing, which the desk camera can
 *   check and a host test cannot.
 *
 *   The rule: the renderer FORMATS, it never COMPUTES.
 */
#include <stdio.h>
#include <string.h>

#include "../pensebem.h"
#include "../pbgame.h"

static int failures;

#define CHECK(cond, ...)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("  FAIL " __VA_ARGS__);                                     \
            printf("\n");                                                      \
            failures++;                                                        \
        }                                                                      \
    } while (0)

/* Answers the current question correctly. */
static PbVerdict answer_right(PbGame *g)
{
    return pb_game_answer(g, pb_game_correct(g));
}

/* Answers the current question with a letter that is NOT the right one. */
static PbVerdict answer_wrong(PbGame *g)
{
    char right = pb_game_correct(g);
    char c = (right == 'A') ? 'B' : 'A';
    return pb_game_answer(g, c);
}

int main(void)
{
    PbGame g;
    int i, q;

    pb_init();

    /* ---- starting a session ------------------------------------------- */

    CHECK(!pb_game_start(&g, 0, 1, 1), "book 00 must be refused");
    CHECK(!pb_game_start(&g, 1, 0, 1), "section 0 must be refused");
    CHECK(!pb_game_start(&g, 100, 1, 1), "book 100 must be refused");
    /* Books 17/18/19 are REAL — the fixture holds 61 observed answers. */
    CHECK(pb_game_start(&g, 17, 1, 1), "book 17 must start (61 real rows exist)");
    CHECK(pb_game_start(&g, 18, 6, 1), "book 18 section 6 must start");

    CHECK(pb_game_start(&g, 1, 1, 1), "book 01 section 1 must start");
    CHECK(g.n == PB_PER_ROUND, "a session is %d questions, got %d", PB_PER_ROUND, g.n);
    CHECK(g.index == 0, "a fresh session starts at index 0");
    CHECK(g.attempt == 1, "a fresh session starts on attempt 1");
    CHECK(g.raw == 0, "a fresh session scores 0");
    CHECK(!g.done, "a fresh session is not done");
    CHECK(pb_game_question(&g) == 1, "book 01 section 1 starts at question 1");

    /* Section 3 must start at question 61 — asserted BY NAME, because at
       section 1 the (s-1)*30 term is zero and cannot detect a wrong constant. */
    CHECK(pb_game_start(&g, 1, 3, 1), "section 3 must start");
    CHECK(pb_game_question(&g) == 61, "section 3 must begin at question 61");
    CHECK(pb_game_start(&g, 1, 5, 1), "section 5 must start");
    CHECK(pb_game_question(&g) == 121, "section 5 must begin at question 121");

    /* ---- the correct answer comes from the formula, not from the game -- */

    CHECK(pb_game_start(&g, 42, 2, 1), "book 42 section 2 must start");
    CHECK(pb_game_correct(&g) == pb_answer(42, 31),
          "the game must report the FORMULA's answer for its current question");

    /* ---- first try: 10 points, advance, attempt resets ----------------- */

    CHECK(pb_game_start(&g, 1, 1, 1), "restart");
    CHECK(answer_right(&g) == PB_RIGHT, "a correct first try must be PB_RIGHT");
    CHECK(pb_game_last_points(&g) == 10, "first try scores 10, got %d", pb_game_last_points(&g));
    CHECK(g.raw == 10, "score after one first-try answer must be 10, got %d", g.raw);
    CHECK(g.index == 1, "a correct answer must advance");
    CHECK(g.attempt == 1, "attempt must reset on advance");
    CHECK(pb_game_question(&g) == 2, "next question must be 2");

    /* ---- second try: 6 points ------------------------------------------ */

    CHECK(pb_game_start(&g, 1, 1, 1), "restart");
    q = pb_game_question(&g);
    CHECK(answer_wrong(&g) == PB_RETRY, "a first wrong answer must be PB_RETRY");
    CHECK(pb_game_last_points(&g) == 0, "a wrong answer scores 0");
    CHECK(g.attempt == 2, "a wrong answer must move to attempt 2");
    CHECK(g.index == 0, "a wrong answer must NOT advance");
    CHECK(pb_game_question(&g) == q, "a wrong answer must stay on the same question");
    CHECK(answer_right(&g) == PB_RIGHT, "correct on try 2 must be PB_RIGHT");
    CHECK(pb_game_last_points(&g) == 6, "second try scores 6, got %d", pb_game_last_points(&g));
    CHECK(g.raw == 6, "score must be 6");

    /* ---- third try: 4 points ------------------------------------------- */

    CHECK(pb_game_start(&g, 1, 1, 1), "restart");
    answer_wrong(&g);
    CHECK(answer_wrong(&g) == PB_RETRY, "a second wrong answer must still be PB_RETRY");
    CHECK(g.attempt == 3, "must be on attempt 3");
    CHECK(answer_right(&g) == PB_RIGHT, "correct on try 3 must be PB_RIGHT");
    CHECK(pb_game_last_points(&g) == 4, "third try scores 4, got %d", pb_game_last_points(&g));

    /* ---- three wrong: reveal, 0 points, advance ------------------------ */

    CHECK(pb_game_start(&g, 1, 1, 1), "restart");
    q = pb_game_question(&g);
    answer_wrong(&g);
    answer_wrong(&g);
    {
        char shown = pb_game_correct(&g);
        CHECK(answer_wrong(&g) == PB_REVEAL, "a third wrong answer must be PB_REVEAL");
        CHECK(pb_game_last_points(&g) == 0, "a revealed question scores 0");
        CHECK(g.raw == 0, "score stays 0");
        CHECK(g.index == 1, "a reveal must advance");
        CHECK(g.attempt == 1, "attempt must reset after a reveal");
        CHECK(pb_game_revealed(&g) == shown,
              "the revealed letter must be the answer to the question just failed");
    }

    /* ---- ⚠ AN INVALID KEY MUST NOT BURN A TRY -------------------------- */

    CHECK(pb_game_start(&g, 1, 1, 1), "restart");
    CHECK(pb_game_answer(&g, 'Z') == PB_IGNORED, "'Z' is not an answer");
    CHECK(pb_game_answer(&g, 0) == PB_IGNORED, "NUL is not an answer");
    CHECK(g.attempt == 1, "an invalid key must NOT consume an attempt");
    CHECK(g.index == 0, "an invalid key must not advance");
    CHECK(g.raw == 0, "an invalid key must not score");

    /* Lowercase is the same key. */
    CHECK(pb_game_start(&g, 1, 1, 1), "restart");
    CHECK(pb_game_answer(&g, (char)(pb_game_correct(&g) + 32)) == PB_RIGHT,
          "answers must be case-insensitive");

    /* ---- a full perfect round ------------------------------------------ */

    CHECK(pb_game_start(&g, 7, 4, 1), "book 07 section 4 must start");
    for (i = 0; i < PB_PER_ROUND; i++) {
        CHECK(!g.done, "session ended early at question %d", i);
        CHECK(pb_game_question(&g) == 91 + i, "section 4 question %d must be %d", i, 91 + i);
        answer_right(&g);
    }
    CHECK(g.done, "a session must end after %d questions", PB_PER_ROUND);
    CHECK(g.raw == 300, "a perfect round scores 300 raw, got %d", g.raw);
    CHECK(pb_game_normalized(&g) == 100, "a perfect round normalises to 100");
    CHECK(pb_game_band(&g) == 0, "a perfect round is band 0");
    CHECK(pb_game_question(&g) == 0, "a finished session has no current question");
    CHECK(pb_game_answer(&g, 'A') == PB_IGNORED, "a finished session must ignore input");

    /* ---- a full round answered wrong three times every time ------------ */

    CHECK(pb_game_start(&g, 7, 1, 1), "restart");
    for (i = 0; i < PB_PER_ROUND; i++) {
        answer_wrong(&g);
        answer_wrong(&g);
        answer_wrong(&g);
    }
    CHECK(g.done, "a session of all-reveals must still end");
    CHECK(g.raw == 0, "an all-wrong round scores 0");
    CHECK(pb_game_band(&g) == 3, "an all-wrong round is band 3");

    /* ---- ⚠ ALL FOUR BANDS MUST BE REACHABLE ---------------------------- */
    /* Banding a raw 0-300 score directly would make two of the four songs
       unreachable and the toy would only ever play half its music. */
    {
        int bands[4];
        memset(bands, 0, sizeof(bands));
        for (i = 0; i <= PB_PER_ROUND; i++) {
            int n;
            CHECK(pb_game_start(&g, 3, 1, 1), "restart");
            for (n = 0; n < PB_PER_ROUND; n++) {
                if (n < i) {
                    answer_right(&g);
                } else {
                    answer_wrong(&g);
                    answer_wrong(&g);
                    answer_wrong(&g);
                }
            }
            bands[pb_game_band(&g)]++;
        }
        for (i = 0; i < 4; i++) {
            CHECK(bands[i] > 0, "band %d unreachable across all %d outcomes", i, PB_PER_ROUND + 1);
        }
    }

    /* ⚠⚠ "ALL FOUR BANDS ARE REACHABLE" IS A VACUOUS ASSERTION ON ITS OWN, and
     *   the red control proved it: banding the RAW score instead of the
     *   normalised one ALSO reaches all four (raw runs 0..300 and the top
     *   boundary is 76), so the suite above stayed GREEN under a real rule
     *   change. The two sides share the term "some band came out" and co-vary.
     *
     *   The fix: name the input where the
     *   two implementations must DISAGREE. 8 correct of 30 is raw 80 -> the raw
     *   reading calls that band 0 (OTIMO); the correct normalised reading is
     *   26% -> band 2. Every boundary below is pinned by name for that reason. */
    {
        struct { int right; int band; } cases[] = {
            { 30, 0 },  /* 100% */
            { 23, 0 },  /*  76% — the exact boundary */
            { 22, 1 },  /*  73% */
            { 16, 1 },  /*  53% */
            { 15, 2 },  /*  50% */
            {  8, 2 },  /*  26% — ⚠ raw 80 would read as band 0. THE discriminator. */
            {  7, 3 },  /*  23% */
            {  0, 3 }   /*   0% */
        };
        unsigned c;
        for (c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
            int n;
            CHECK(pb_game_start(&g, 3, 1, 1), "restart");
            for (n = 0; n < PB_PER_ROUND; n++) {
                if (n < cases[c].right) {
                    answer_right(&g);
                } else {
                    answer_wrong(&g);
                    answer_wrong(&g);
                    answer_wrong(&g);
                }
            }
            CHECK(pb_game_band(&g) == cases[c].band,
                  "%d/30 right = raw %d = %d%% must be band %d, got %d",
                  cases[c].right, g.raw, pb_game_normalized(&g), cases[c].band,
                  pb_game_band(&g));
        }
    }

    /* ---- section 6 is a real review round ------------------------------ */

    CHECK(pb_game_start(&g, 5, 6, 12345), "book 05 section 6 must start");
    CHECK(g.n == PB_PER_ROUND, "review round is %d questions", PB_PER_ROUND);
    for (i = 0; i < PB_PER_ROUND; i++) {
        int lo = i * PB_PAGE + 1;
        CHECK(g.questions[i] >= lo && g.questions[i] <= lo + PB_PAGE - 1,
              "review q %d = %d, want page %d (%d..%d)", i, g.questions[i], i, lo, lo + PB_PAGE - 1);
        if (i > 0) {
            CHECK(g.questions[i] > g.questions[i - 1], "review must ascend at %d", i);
        }
    }
    /* Different seeds must give different rounds, or "review" is a fixed list. */
    {
        int a[PB_PER_ROUND];
        pb_game_start(&g, 5, 6, 999);
        memcpy(a, g.questions, sizeof(a));
        pb_game_start(&g, 5, 6, 4242);
        CHECK(memcmp(a, g.questions, sizeof(a)) != 0,
              "two different seeds produced an identical review round");
    }
    /* And the answer still comes from the formula, at the REVIEW question. */
    pb_game_start(&g, 5, 6, 777);
    CHECK(pb_game_correct(&g) == pb_answer(5, g.questions[0]),
          "review mode must ask the formula about the REVIEW question number");

    printf("GAME_FAILURES=%d\n", failures);
    return failures == 0 ? 0 : 1;
}

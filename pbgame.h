/* pbgame.h — the Pense Bem book-mode session, as a pure state machine.
 *
 * Same portability contract as pensebem.h: compiled by BOTH
 * xtensa-esp32-elf-g++ (via the .ino) and cc -std=c99 -Werror (via
 * game-test/pb-game.c). No <Arduino.h>, no allocation, no I/O, no time.
 *
 * ⚠ WHY THE RULES LIVE HERE AND NOT IN THE SKETCH:
 *   scoring, retry counting and question advance are exactly the parts a player
 *   could never notice being wrong — a toy that quietly pays 10 points for a
 *   second-try answer looks identical to one that pays 6. None of it needs a
 *   screen, a button or a millisecond, so none of it belongs anywhere a host
 *   test cannot reach. The .ino keeps input mapping and drawing, which the desk
 *   camera CAN check and a host test cannot.
 *
 *   The rule this encodes: THE RENDERER FORMATS, IT NEVER COMPUTES.
 *
 * Written test-first: game-test/pb-game.c existed and was RED before this file.
 */
#ifndef PBGAME_H
#define PBGAME_H

#include "pensebem.h"

typedef enum {
    PB_RIGHT = 0,  /* correct; points awarded, advanced                        */
    PB_RETRY,      /* wrong, tries left; same question, attempt incremented    */
    PB_REVEAL,     /* wrong on the last try; answer revealed, advanced, 0 pts  */
    PB_IGNORED     /* not an answer key, or the session is over — NO state change */
} PbVerdict;

typedef struct {
    int  book;
    int  section;
    int  questions[PB_PER_ROUND]; /* absolute question numbers, in play order */
    int  n;                       /* always PB_PER_ROUND once started         */
    int  index;                   /* 0..n-1, or n when done                   */
    int  attempt;                 /* 1..3                                     */
    int  raw;                     /* 0..300                                   */
    int  done;
    int  last_points;             /* points from the most recent answer       */
    char revealed;                /* letter shown after a 3rd wrong try, else 0 */
} PbGame;

#define PB_MAX_ATTEMPTS 3

/* Starts a session. Returns 1, or 0 and leaves *g untouched on a bad code.
 *
 * ⚠ Books 17/18/19 START. They are real books — the harvested fixture holds 61
 *   observed answers for them. Only book 00 and section 0 are refused. See the
 *   note on pb_answer() in pensebem.h. */
static inline int pb_game_start(PbGame *g, int book, int section, unsigned long long seed)
{
    int i, n;

    if (g == 0 || !pb_code_valid(book, section)) {
        return 0;
    }
    n = pb_questions_for(section, seed, g->questions);
    if (n != PB_PER_ROUND) {
        return 0;
    }
    g->book = book;
    g->section = section;
    g->n = n;
    g->index = 0;
    g->attempt = 1;
    g->raw = 0;
    g->done = 0;
    g->last_points = 0;
    g->revealed = 0;
    for (i = n; i < PB_PER_ROUND; i++) {
        g->questions[i] = 0;
    }
    return 1;
}

/* The ABSOLUTE question number to look up in the printed book, or 0 when done.
 *
 * ⚠ This is not the same as the position in the session, and the screen must
 *   show BOTH. In section 6 the numbers jump around the whole book; without the
 *   absolute number, review mode is unusable with a real printed book. */
static inline int pb_game_question(const PbGame *g)
{
    if (g == 0 || g->done || g->index < 0 || g->index >= g->n) {
        return 0;
    }
    return g->questions[g->index];
}

/* The correct letter for the current question, straight from the formula. */
static inline char pb_game_correct(const PbGame *g)
{
    int q = pb_game_question(g);

    if (q == 0) {
        return 0;
    }
    return pb_answer(g->book, q);
}

/* The letter revealed by the most recent 3rd-try failure, else 0. */
static inline char pb_game_revealed(const PbGame *g)
{
    return g ? g->revealed : (char)0;
}

static inline int pb_game_last_points(const PbGame *g)
{
    return g ? g->last_points : 0;
}

static inline int pb_game_normalized(const PbGame *g)
{
    return g ? pb_normalized(g->raw, g->n) : 0;
}

/* 0 = OTIMO, 1 = MUITO BEM, 2 = QUASE LA, 3 = TENTE MAIS.
   ⚠ Normalises first. Banding a raw 0-300 score against FINDINGS' 0-100 bands
   would make two of the four end-of-session songs unreachable. */
static inline int pb_game_band(const PbGame *g)
{
    return pb_band(pb_game_normalized(g));
}

static inline void pb_game_advance_(PbGame *g)
{
    g->index++;
    g->attempt = 1;
    if (g->index >= g->n) {
        g->done = 1;
    }
}

/* Submits an answer. 'A'-'D' or 'a'-'d'.
 *
 * ⚠ ANYTHING ELSE RETURNS PB_IGNORED AND CHANGES NOTHING — it must never burn a
 *   try. On a device driven by one SELECT button cycling through values, a
 *   stray or mis-timed key that silently cost an attempt would look exactly like
 *   the player getting it wrong, and would quietly change the score. */
static inline PbVerdict pb_game_answer(PbGame *g, char choice)
{
    char c, right;

    if (g == 0 || g->done) {
        return PB_IGNORED;
    }
    c = pb_upper(choice);
    if (c < 'A' || c > 'D') {
        return PB_IGNORED;
    }
    right = pb_game_correct(g);
    if (right == 0) {
        return PB_IGNORED;
    }

    if (c == right) {
        g->last_points = pb_points_for_attempt(g->attempt);
        g->raw += g->last_points;
        g->revealed = 0;
        pb_game_advance_(g);
        return PB_RIGHT;
    }

    g->last_points = 0;
    if (g->attempt < PB_MAX_ATTEMPTS) {
        g->attempt++;
        return PB_RETRY;
    }

    g->revealed = right;
    pb_game_advance_(g);
    return PB_REVEAL;
}

#endif /* PBGAME_H */

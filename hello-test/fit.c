/* fit.c — does the disclosure actually FIT on the panel?
 *
 * ⚠ THIS IS THE ONLY USER-FACING SURFACE IN THE PROJECT WITH NO EYES ON IT.
 *
 * centreFit() steps a string down through font sizes until it fits — but
 * T_SMALL is the FLOOR. Below FreeSans9pt there is nothing, so a body line that
 * is too wide does not shrink: it CLIPS at the panel edge, silently. Two strings
 * have already shipped on this exact board that way and both were found by
 * pointing a camera at it, not by any test. A consent notice is the worst place
 * for that to happen again.
 *
 * So this measures every line with the REAL TFT_eSPI font tables the firmware
 * links against, summing xAdvance exactly as TFT_eSPI::textWidth() does for a
 * GFXfont — and it reads the strings from pbhello.h, the same header the sketch
 * includes, so the two cannot drift.
 *
 * ⚠ It measures WIDTH ONLY. It cannot tell you the screen reads well, that the
 * lines do not overlap visually, or that the contrast works. The camera check
 * still has to happen; this just means the camera is not the FIRST thing to
 * notice a clipped line.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define PROGMEM
typedef struct { uint16_t bitmapOffset; uint8_t width, height, xAdvance; int8_t xOffset, yOffset; } GFXglyph;
typedef struct { uint8_t *bitmap; GFXglyph *glyph; uint16_t first, last; uint8_t yAdvance; } GFXfont;

#include "FreeSans9pt7b.h"
#include "FreeSansBold12pt7b.h"
#include "../pbhello.h"

/* From the sketch: centreFit(s, y, W - 8, tier) with W = 240. */
#define MAXW 232
/* Below this much slack a line is one word-edit away from clipping, and nothing
   would notice until someone looked at hardware. Treated as a failure. */
#define MIN_SLACK 12

static int fails = 0;

static int text_width(const char *s, const GFXfont *f, int report)
{
    int w = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c < f->first || c > f->last) {
            /* ⚠ A character outside the font renders as NOTHING and costs NO
               width — the silent-drop failure this board already shipped once,
               with a clock font that contained no letters. Never let it pass. */
            if (report) {
                printf("      ⚠ char 0x%02x ('%c') IS NOT IN THIS FONT — renders as nothing\n",
                       c, (c >= 32 && c < 127) ? c : '?');
                fails++;
            }
            continue;
        }
        w += f->glyph[c - f->first].xAdvance;
    }
    return w;
}

/* Mirrors centreFit()'s tier fallback.
   T_MED (title): try FreeSansBold12pt, else drop to FreeSans9pt.
   T_SMALL (body): FreeSans9pt only — NO fallback exists below it. */
static void check(const char *s, int is_title)
{
    const GFXfont *SMALL = &FreeSans9pt7b;
    const GFXfont *MED   = &FreeSansBold12pt7b;
    const char *face = "9pt";
    int w;

    if (is_title) {
        w = text_width(s, MED, 1);
        if (w <= MAXW) face = "12ptB";
        else { w = text_width(s, SMALL, 0); face = "9pt(fell back)"; }
    } else {
        w = text_width(s, SMALL, 1);
    }

    int slack = MAXW - w;
    const char *verdict = slack < 0 ? "CLIPS" : (slack < MIN_SLACK ? "TIGHT" : "ok");
    printf("  %-15s %3dpx  slack %+4dpx  %-5s  \"%s\"\n", face, w, slack, verdict, s);

    if (slack < 0) {
        printf("        ^^ RUNS OFF THE PANEL — there is no smaller font to fall back to\n");
        fails++;
    } else if (slack < MIN_SLACK) {
        printf("        ^^ under %dpx of margin: too close to ship unseen\n", MIN_SLACK);
        fails++;
    }
}

static void page(const char *name, const char *const *lines)
{
    printf("%s\n", name);
    for (int i = 0; i < PB_HELLO_LINES; i++) check(lines[i], i == 0);
    printf("\n");
}

int main(void)
{
    printf("panel is 240px wide; centreFit receives W-8 = %dpx\n\n", MAXW);

    page("PORTUGUESE", PB_HELLO_PT);
    page("ENGLISH",    PB_HELLO_EN);

    /* Vertical budget: title at y=4, body lines from y=26 at 17px pitch. */
    int last_bottom = 26 + (PB_HELLO_LINES - 2) * 17 + FreeSans9pt7b.yAdvance;
    printf("vertical: last line ends at y=%d, panel is 135 -> %s\n",
           last_bottom, last_bottom <= 135 ? "ok" : "OVERFLOWS");
    if (last_bottom > 135) fails++;

    if (fails) { printf("\n%d LINE(S) NEED ATTENTION\n", fails); return 1; }
    printf("\nevery line fits with at least %dpx of margin\n", MIN_SLACK);
    return 0;
}

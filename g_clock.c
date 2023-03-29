#include "g_local.h"

/**
 * Advance the arena's clock
 */
void G_ClockTick(arena_t *a)
{
    if (!a) {
        return;
    }

    if (!a->clock.think) {
        return;
    }

    if (a->clock.nextthink > level.framenum) {
        return;
    }

    a->clock.think(a);
}

/**
 * Each arena clock runs this once per second
 */
void G_ClockThink(void *p)
{
    arena_t *a = (arena_t *)p;
    arena_clock_t *c = &a->clock;

    switch (c->type) {
    case CLOCK_NONE:
        if (c->string[0] == 0) {
            strncpy(c->string, "00:00", sizeof(c->string));
        }
        return;
    case CLOCK_COUNTUP:
        c->value++;
        G_SecsToString(c->string, c->value);
        break;
    case CLOCK_COUNTDOWN:
        c->value--;
        if (c->value == 0) {
            if (c->complete) {
                c->complete(p);
                c->complete = NULL;
            }
            c->type = CLOCK_NONE;
            return;
        }
        G_SecsToString(c->string, c->value);
        break;
    }

    c->nextthink = level.framenum + SECS_TO_FRAMES(1);
    G_UpdateConfigStrings(a);
}

/**
 * This is a clock think function for round countdowns.
 * Also works for the end of a timeout.
 *
 * Assumes clock type is COUNTDOWN
 */
void G_RoundCountdownThink(void *p)
{
    arena_t *a = (arena_t *)p;
    arena_clock_t *c = &a->clock;

    if (c->type == CLOCK_NONE) {
        return;
    }

    c->value--;

    G_SecsToString(c->string, c->value);

    if (c->value < 1) {
        c->type = CLOCK_NONE;
        if (c->complete) {
            c->complete(p);
            c->complete = NULL;
        }
        return;
    }

    G_Centerprintf(a, "%d", c->value);

    if (c->value < 10) {
        G_ArenaSound(a, level.sounds.countdown[c->value]);
    }

    c->nextthink = level.framenum + SECS_TO_FRAMES(1);
    G_UpdateConfigStrings(a);
    gi.dprintf("%s\n", c->string);
}

/**
 * Each second of an intermission
 */
void G_RoundIntermissionThink(void *p)
{
    arena_t *a = (arena_t *)p;
    arena_clock_t *c = &a->clock;

    if (c->type == CLOCK_NONE) {
        return;
    }

    c->value--;

    G_SecsToString(c->string, c->value);

    if (c->value < 1) {
        c->type = CLOCK_NONE;
        if (c->complete) {
            c->complete(p);
            c->complete = NULL;
        }
        c->type = CLOCK_NONE;
        return;
    }

    c->nextthink = level.framenum + SECS_TO_FRAMES(1);
    gi.dprintf("%s\n", c->string);
}
/**
 * A simple test function to run at the end of a countdown clock
 */
void G_AlarmTest(void *p)
{
    arena_t *a = (arena_t *)p;
    gi.dprintf("COUNTDOWN OVER! (arena %d)\n", a->number);
}



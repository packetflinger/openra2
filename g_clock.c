#include "g_local.h"

/**
 * Setup a new clock
 */
void ClockInit(arena_clock_t *c, void *arena, char *name, uint32_t start, uint32_t end, clock_type_t dir) {
    memset(c, 0, sizeof(arena_clock_t));
    strncpy(c->name, name, sizeof(c->name));
    c->arena = arena;
    c->startvalue = start;
    c->endvalue = end;
    c->value = start;
    c->direction = dir;
    c->thinkinterval = HZ;
    c->tick = (void *) ClockThink;
}

/**
 * Advance a clock
 */
void ClockThink(arena_clock_t *c) {
    if (!c) {
        return;
    }

    if (c->state == CLOCK_STATE_STOPPED) {
        return;
    }

    if (c->nextthink > level.framenum) {
        return;
    }

    if (c->direction == CLOCK_UP) {
        c->value++;
        G_SecsToString(c->string, c->value);
    } else {
        c->value--;
        G_SecsToString(c->string, c->value - c->endvalue);
    }

    c->nextthink = level.framenum + c->thinkinterval;

    if (c->postthink) {
        (c->postthink)(c);
    }

    if (c->value == c->endvalue) {
        c->state = CLOCK_STATE_STOPPED;
        if (c->finish) {
            gi.dprintf("finish arena value: %p\n", c->arena);
            c->finish(c, c->arena);
        }
    }
}

/**
 * Start the clock
 */
void ClockStart(arena_clock_t *c) {
    c->state = CLOCK_STATE_RUNNING;
    c->nextthink = level.framenum + 1;   // start immediately
}

/**
 * Stop the clock
 * It can be started again from where it left off
 */
void ClockStop(arena_clock_t *c) {
    c->state = CLOCK_STATE_STOPPED;
}

/**
 * Set back to starting point
 */
void ClockReset(arena_clock_t *c) {
    c->state = CLOCK_STATE_STOPPED;
    c->value = c->startvalue;
}

/**
 * Called just after the clock is advanced each time.
 * This is just for testing.
 */
void ClockTestPostThink(arena_clock_t *c) {
    gi.dprintf("%s (%s)\n", c->string, c->name);
}

/**
 * Called once when the clock has finished it's run.
 * this is just for testing
 */
void ClockTestFinish(arena_clock_t *c) {
    gi.dprintf("clock[%s] finished!\n", c->name);
    if (c->direction == CLOCK_UP) {
        ClockReset(c);
        c->direction = CLOCK_DOWN;
        c->startvalue = 10;
        c->value = 10;
        c->endvalue = 0;
    } else {
        ClockReset(c);
        c->direction = CLOCK_UP;
        c->startvalue = 5;
        c->value = 5;
        c->endvalue = 12;
    }
    c->thinkinterval = HZ;
    c->tick = (void *) ClockThink;
    c->postthink = (void *) ClockTestPostThink;
    c->finish = (void *) ClockTestFinish;
    ClockStart(c);
}

/**
 * Test a match countdown timer
 */
void ClockTestMatchCountdown(arena_clock_t *c) {
    gi.dprintf("match start!\n");
    ClockReset(c);
    ClockInit(c, NULL, "match", 30, 0, CLOCK_DOWN);
    c->postthink = (void *) ClockTestPostThink;
    c->finish = (void *) ClockTestMatchFinish;
    ClockStart(c);
}

/**
 * Test end of match timer
 */
void ClockTestMatchFinish(arena_clock_t *c) {
    gi.dprintf("match end!\n");
    ClockReset(c);
}

/**
 * Start a round countdown timer
 */
void ClockStartRoundCountdown(arena_t *a) {
    arena_clock_t *clock;
    if (!a) {
        return;
    }
    clock = &a->clock;
    ClockInit(clock, a, "countdown", a->countdown, 0, CLOCK_DOWN);
    clock->postthink = (void *) ClockTestPostThink;
    clock->finish = (void *) ClockStartRound;
    ClockStart(clock);
}

/**
 *
 */
void ClockStartRound(arena_clock_t *c, arena_t *a) {
    gi.dprintf("starting match!\n");
    G_StartRound(a);
}

/**
 *
 */
void ClockStartIntermission(arena_t *a) {
    arena_clock_t *clock;
    if (!a) {
        return;
    }
    clock = &a->clock;
    ClockInit(clock, a, "intermission", (int)g_round_end_time->value, 0, CLOCK_DOWN);
    clock->postthink = (void *) ClockTestPostThink;
    clock->finish = (void *) ClockEndIntermission;
    ClockStart(clock);
}

/**
 *
 */
void ClockEndIntermission(arena_clock_t *c, arena_t *a) {
    G_EndRoundIntermission(a);
}

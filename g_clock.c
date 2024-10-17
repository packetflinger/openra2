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
 */
void ClockPostThink(arena_clock_t *c) {
    arena_t *a = c->arena;
    if ((int)g_debug_clocks->value) {
        gi.cprintf(NULL, PRINT_HIGH, "%s (%s)\n", c->string, c->name);
    }

    // map timelimit clocks aren't linked to a specific arena
    if (!a) {
        return;
    }

    G_UpdateConfigStrings(a);
    if (a->state == ARENA_STATE_COUNTDOWN) {
        a->countdown = c->value; // for center screen number
        if (c->value > 0 && c->value <= 10) {
            G_ArenaSound(a, level.sounds.countdown[c->value]);
        }
    }
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
    c->postthink = (void *) ClockPostThink;
    c->finish = (void *) ClockTestFinish;
    ClockStart(c);
}

/**
 * Test a match countdown timer
 */
void ClockTestMatchCountdown(arena_clock_t *c) {
    ClockReset(c);
    ClockInit(c, NULL, "match", 30, 0, CLOCK_DOWN);
    c->postthink = (void *) ClockPostThink;
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
    clock->postthink = (void *) ClockPostThink;
    clock->finish = (void *) ClockStartRound;
    ClockStart(clock);
}

/**
 *
 */
void ClockStartRound(arena_clock_t *c, arena_t *a) {
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
    clock->postthink = (void *) ClockPostThink;
    clock->finish = (void *) ClockEndIntermission;
    ClockStart(clock);
}

/**
 *
 */
void ClockEndIntermission(arena_clock_t *c, arena_t *a) {
    G_EndRoundIntermission(a);
}

/**
 *
 */
void ClockStartTimeout(arena_t *a) {
    arena_clock_t *clock;
    if (!a) {
        return;
    }
    clock = &a->timeout_clock;
    ClockInit(clock, a, "timeout", (int)g_timeout_time->value, 0, CLOCK_DOWN);
    clock->postthink = (void *) ClockPostThink;
    clock->finish = (void *) ClockEndTimeout;
    ClockStart(clock);
}

/**
 *
 */
void ClockEndTimeout(arena_clock_t *c, arena_t *a) {
    a->state = ARENA_STATE_PLAY;
    a->timeout_caller = NULL;
    G_ArenaSound(a, level.sounds.timein);
}

/**
 *
 */
void ClockStartMatchIntermission(arena_t *a) {
    arena_clock_t *clock;
    if (!a) {
        return;
    }
    clock = &a->clock;
    ClockInit(clock, a, "match intermission", (int)g_intermission_time->value, 0, CLOCK_DOWN);
    clock->postthink = (void *) ClockPostThink;
    clock->finish = (void *) ClockEndMatchIntermission;
    ClockStart(clock);
}

/**
 *
 */
void ClockEndMatchIntermission(arena_clock_t *c, arena_t *a) {
    if (a->state == ARENA_STATE_MINTERMISSION) {
        G_ResetArena(a);
    }
}

/**
 * Callback for when the timelimit clock is finished, change to next map.
 * The arena_t arg should be NULL for this one.
 */
void ClockEndNextMap(arena_clock_t *c, arena_t *a) {
    if (!c) {
        gi.dprintf("%s called with null clock value\n", __func__);
        return;
    }
    if ((int)g_debug_clocks->value) {
        gi.dprintf("The %s clock just finished\n", c->name);
    }
    gi.bprintf(PRINT_HIGH, "Timelimit hit\n");
    G_EndLevel();
}

/**
 * The main "timelimit" cvar affects how long a map is active. Once that timer
 * runs out, everyone should go to intermission (even mid round) and the server
 * should proceed to the next map.
 */
void ClockStartMapTimelimit(int secs) {
    arena_clock_t *c = &level.clock;
    ClockInit(c, NULL, "Map Timelimit", secs, 0, CLOCK_DOWN);
    c->postthink = (void *) ClockPostThink;
    c->finish = (void *) ClockEndNextMap;
    ClockStart(c);
}

/**
 * Callback for when the level-end intermission is finished, change to next map.
 * The arena_t arg should be NULL for this one.
 */
void ClockEndChangeMap(arena_clock_t *c, arena_t *a) {
    if (!c) {
        gi.dprintf("%s called with null clock value\n", __func__);
        return;
    }
    if ((int)g_debug_clocks->value) {
        gi.dprintf("The %s clock just finished\n", c->name);
    }
    gi.AddCommandString(va("gamemap \"%s\"\n", level.nextmap));
}

/**
 * Pause for a few seconds at a server-level intermission, then change to the
 * next map.
 */
void ClockStartEndLevelIntermission(int secs) {
    arena_clock_t *c = &level.clock;
    ClockInit(c, NULL, "End Level Intermission", secs, 0, CLOCK_DOWN);
    c->postthink = (void *) ClockPostThink;
    c->finish = (void *) ClockEndChangeMap;
    ClockStart(c);
}

#pragma once

#include "g_local.h"

#define CLOCK_MAX_NAME      50
#define CLOCK_MAX_STRING    9

/**
 * Whether a clock counts up to some limit or
 * down to zero
 */
typedef enum {
    CLOCK_UP,
    CLOCK_DOWN,
} clock_type_t;

/**
 * What the clock is currently doing
 */
typedef enum {
    CLOCK_STATE_STOPPED,
    CLOCK_STATE_RUNNING,
} clock_state_t;

/**
 * Generic clock structure. Can use be used for a countdown timer
 * or just a general purpose clock.
 */
typedef struct {
    char name[CLOCK_MAX_NAME];  // just an identifier
    void *arena;                // which arena is this clock in?
    clock_type_t direction;     // up or down
    clock_state_t state;        // what's it currently doing?
    uint32_t startvalue;        // initial value
    uint32_t endvalue;          // when to stop
    uint32_t value;             // the current value (ticks/seconds)
    uint32_t nextthink;         // next frame to do something on
    uint32_t thinkinterval;     // how often do we think?
    char string[9];             // something like "00:xx" format
    void (*tick)(void *c);      // function to advance the clock
    void (*postthink)(void *c); // called after tick, decides what should happen next
    void (*finish)(void *c, void *a);    // run when timer is finished
} arena_clock_t;

void ClockInit(arena_clock_t *c, void *arena, char *name, uint32_t start, uint32_t end, clock_type_t dir);
void ClockDestroy(arena_clock_t *c);
void ClockThink(arena_clock_t *c);
void ClockStart(arena_clock_t *c);
void ClockStop(arena_clock_t *c);
void ClockReset(arena_clock_t *c);
void ClockTestMatchCountdown(arena_clock_t *c);
void ClockTestMatchFinish(arena_clock_t *c);
void ClockTestPostThink(arena_clock_t *c);
void ClockTestFinish(arena_clock_t *c);


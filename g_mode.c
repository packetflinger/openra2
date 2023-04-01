#include "g_local.h"

/**
 * Standard team-mode frame
 *
 * 2 or more teams, if you're fragged you're out. Match is
 * over when only 1 team has players left alive.
 */
void G_TeamFrame(void *p)
{
    arena_t *a = (arena_t *)p;

    // don't bother if nobody is here
    if (a->player_count == 0) {
        return;
    }

    // start match countdown
    if (a->state == ARENA_STATE_WARMUP) {
        if (a->ready) {    // is everyone ready?
            G_StartMatch(a);
        }
    }

    // end of round
    if (ROUNDOVER(a)) {
        gi.dprintf("Should start intermission\n");
        G_BeginRoundIntermission(a);
    }

    G_ClockTick(a);

    if (a->state > ARENA_STATE_WARMUP) {
        a->round_frame++;
        a->match_frame++;
    }

    // end of round
    /*
    if (a->players_alive <= 1 && MATCHPLAYING(a)) {
        G_BeginRoundIntermission(a);
    }

    // check vote timeout
    if (level.vote.proposal) {
        G_UpdateVote();
    }

    // start match
    if (a->state < ARENA_STATE_PLAY && a->round_start_frame) {
        int framesleft = a->round_start_frame - level.framenum;

        if (framesleft > 0 && framesleft % SECS_TO_FRAMES(1) == 0) {
            a->countdown = FRAMES_TO_SECS(framesleft);
        } else if (framesleft == 0) {
            //G_StartRound(a);
            return;
        }
    }

    // start match countdown
    if (a->state == ARENA_STATE_WARMUP) {
        if (a->ready) {    // is everyone ready?
            G_StartRoundCountdown(a);
        }
    }

    G_CheckIntermission(a);

    // end of round
    if (ROUNDOVER(a)) {
        G_BeginRoundIntermission(a);
    }

    //G_CheckTimers(a);
    G_UpdateArenaVote(a);
    G_CheckArenaRules(a);
    */
}

/**
 * A single team all fighting each other. Last player
 * alive wins the round
 */
void G_FFAFrame(void *p)
{
    //arena_t *a = (arena_t *)p;
}

/**
 * Red Rover mode is team-based where as players are
 * fragged, they respawn on the team that fragged them.
 * This flip-flopping of teams continues until only 1
 * team has players left alive.
 */
void G_RoverFrame(void *p)
{
    //arena_t *a = (arena_t *)p;
}

#include "g_local.h"

/**
 * Get a pointer to the team of type in an arena
 */
arena_team_t *FindTeam(arena_t *a, arena_team_type_t type) {
    uint8_t i;

    if (!a) {
        return NULL;
    }
    for (i=0; i<a->team_count; i++) {
        if (a->teams[i].type == type) {
            return &a->teams[i];
        }
    }
    return NULL;
}

/**
 * Add player to a team
 */
void G_TeamJoin(edict_t *ent, arena_team_type_t type, qboolean forced) {
    if (!ent->client) {
        return;
    }

    arena_t *arena = ARENA(ent);
    arena_team_t *team = FindTeam(arena, type);

    PMenu_Close(ent);

    if (!arena) {
        gi.cprintf(ent, PRINT_HIGH, "Unknown arena\n");
        return;
    }

    if (!team) {
        gi.cprintf(ent, PRINT_HIGH, "Unknown team, can't join it\n");
        return;
    }

    // match has started, can't join
    if (arena->state >= ARENA_STATE_PLAY && !forced) {
        gi.cprintf(ent, PRINT_HIGH, "Match in progress, you can't join now\n");
        return;
    }

    if (team->locked && !forced) {
        gi.cprintf(ent, PRINT_HIGH, "Team %s is locked\n", team->name);
        return;
    }

    if (team->player_count == MAX_ARENA_TEAM_PLAYERS) {
        gi.cprintf(ent, PRINT_HIGH, "Team %s is full\n", team->name);
        return;
    }

    if (TEAM(ent)) {
        if (TEAM(ent)->type == type) {
            G_TeamPart(ent, false);
            return;
        } else {
            G_TeamPart(ent, false);
        }
    }

    // add player to the team
    TEAM(ent) = team;
    team->player_count++;

    int i;
    for (i = 0; i < MAX_ARENA_TEAM_PLAYERS; i++) {
        if (!team->players[i]) {    // free player slot, take it
            team->players[i] = ent;
            break;
        }
    }

    // remove them from the spectator list
    G_SpectatorsPart(ent);

    // captainless team, promote this player
    if (!team->captain) {
        team->captain = ent;
    }

    G_bprintf(arena, PRINT_HIGH, "%s joined team %s\n", NAME(ent), team->name);
    G_SetSkin(ent);
    spectator_respawn(ent, CONN_SPAWNED);
}

/**
 * Remove this player from whatever team they're on
 */
void G_TeamPart(edict_t *ent, qboolean silent)
{
    arena_team_t *oldteam;
    int i;

    if (!ent->client) {
        return;
    }

    oldteam = TEAM(ent);

    if (!oldteam) {
        return;
    }

    // remove player
    for (i = 0; i < MAX_ARENA_TEAM_PLAYERS; i++) {
        if (oldteam->players[i] == ent) {
            oldteam->players[i] = NULL;
            break;
        }
    }

    // we're the captain, reassign to next player
    if (oldteam->captain == ent) {
        for (i = 0; i < MAX_ARENA_TEAM_PLAYERS; i++) {
            if (oldteam->players[i]) {
                oldteam->captain = oldteam->players[i];
                break;
            }
        }
    }

    oldteam->player_count--;

    if (!silent) {
        G_bprintf(ARENA(ent), PRINT_HIGH, "%s left team %s\n",
                NAME(ent), oldteam->name);
    }

    ent->client->pers.ready = false;
    ent->client->pers.team = 0;

    G_SpectatorsJoin(ent);

    // if everyone else is already ready, start the match
    G_CheckTeamReady(oldteam);
    G_CheckArenaReady(ARENA(ent));

    spectator_respawn(ent, CONN_SPECTATOR);
}

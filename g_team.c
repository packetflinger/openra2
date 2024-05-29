#include "g_local.h"

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

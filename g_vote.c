/*
Copyright (C) 2008-2009 Andrey Nazarov

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "g_local.h"

#define MAY_VOTE(c) \
    (c->pers.connected == CONN_SPAWNED || c->pers.admin || VF(SPECS))

static int G_CalcVote(int *votes)
{
    gclient_t *c;
    int total = 0;

    votes[0] = votes[1] = 0;
    for (c = game.clients; c < game.clients + game.maxclients; c++) {
        if (c->pers.connected <= CONN_CONNECTED) {
            continue;
        }
        if (c->pers.mvdspec) {
            continue;
        }
        if (!MAY_VOTE(c)) {
            continue; // don't count spectators
        }

        total++;
        if (c->level.vote.index != level.vote.index) {
            continue; // not voted yet
        }

        if (c->pers.admin) {
            // admin vote decides immediately
            votes[c->level.vote.accepted    ] = game.maxclients;
            votes[c->level.vote.accepted ^ 1] = 0;
            break;
        }

        // count normal vote
        votes[c->level.vote.accepted]++;
    }

    return total;
}

static int _G_CalcVote(int *votes)
{
    int threshold = (int)g_vote_threshold->value;
    int total = G_CalcVote(votes);

    total = total * threshold / 100 + 1;

    return total;
}

void G_FinishVote(void)
{
    if (VF(SHOW)) {
        gi.configstring(CS_VOTE_PROPOSAL, "");
    }
    level.vote.proposal = 0;
    level.vote.framenum = level.framenum;
    level.vote.victim = NULL;
}

void G_UpdateVote(void)
{
    char buffer[MAX_QPATH];
    int votes[2], total;
    int remaining;

    if (!level.vote.proposal) {
        return;
    }

    // check timeout
    if (level.framenum >= level.vote.framenum) {
        gi.bprintf(PRINT_HIGH, "Vote timed out.\n");
        G_FinishVote();
        return;
    }

    // update vote count every second
    if (!VF(SHOW)) {
        return;
    }

    remaining = level.vote.framenum - level.framenum;
    if (remaining % HZ) {
        return;
    }

    total = _G_CalcVote(votes);
    Q_snprintf(buffer, sizeof(buffer), "Yes: %d (%d) No: %d [%02d sec]",
               votes[1], total, votes[0], remaining / HZ);

    gi.WriteByte(svc_configstring);
    gi.WriteShort(CS_VOTE_COUNT);
    gi.WriteString(buffer);
    gi.multicast(NULL, MULTICAST_ALL);
}

qboolean G_CheckVote(void)
{
    int threshold = (int)g_vote_threshold->value;
    int votes[2], total;
    int acc, rej;

    if (!level.vote.proposal) {
        return qfalse;
    }

    // is vote initiator gone?
    if (!level.vote.initiator->pers.connected) {
        gi.bprintf(PRINT_HIGH, "Vote aborted due to the initiator disconnect.\n");
        goto finish;
    }

    // is vote victim gone?
    if (level.vote.victim && !level.vote.victim->pers.connected) {
        gi.bprintf(PRINT_HIGH, "Vote aborted due to the victim disconnect.\n");
        goto finish;
    }

    // are there any players?
    total = G_CalcVote(votes);
    if (!total) {
        gi.bprintf(PRINT_HIGH, "Vote aborted due to the absence of players.\n");
        goto finish;
    }

    rej = votes[0] * 100 / total;
    acc = votes[1] * 100 / total;

    if (acc > threshold) {
        switch (level.vote.proposal) {

        case VOTE_KICK:
            gi.bprintf(PRINT_HIGH, "Vote passed.\n");
            gi.AddCommandString(va("kick %d\n", (int)(level.vote.victim - game.clients)));
            break;
        case VOTE_MUTE:
            gi.bprintf(PRINT_HIGH, "Vote passed: %s has been muted\n", level.vote.victim->pers.netname);
            level.vote.victim->pers.muted = qtrue;
            break;
        case VOTE_MAP:
            gi.bprintf(PRINT_HIGH, "Vote passed: next map is %s.\n", level.vote.map);
            strcpy(level.nextmap, level.vote.map);
            BeginIntermission();
            break;
        case VOTE_TELEMODE:
            gi.bprintf(PRINT_HIGH, "Vote passed: teleporter mode is now %s.\n",
                       level.vote.value ? "NO FREEZE" : "NORMAL");
            gi.cvar_set("g_teleporter_nofreeze", va("%d", level.vote.value));
            game.settings_modified++;
            break;

        default:
            break;
        }
        goto finish;
    }

    if (rej > threshold) {
        gi.bprintf(PRINT_HIGH, "Vote failed.\n");
        goto finish;
    }

    return qfalse;

finish:
    G_FinishVote();
    return qtrue;
}


static void G_BuildProposal(char *buffer)
{
    switch (level.vote.proposal) {

    case VOTE_KICK:
        sprintf(buffer, "kick %s", level.vote.victim->pers.netname);
        break;
    case VOTE_MUTE:
        sprintf(buffer, "mute %s", level.vote.victim->pers.netname);
        break;
    case VOTE_MAP:
        sprintf(buffer, "map %s", level.vote.map);
        break;
    case VOTE_TELEMODE:
        sprintf(buffer, "%s teleporter mode",
                level.vote.value ? "no freeze" : "normal");
        break;
    default:
        strcpy(buffer, "unknown");
        break;
    }
}

void Cmd_CastVote_f(edict_t *ent, qboolean accepted)
{
    if (!level.vote.proposal) {
        gi.cprintf(ent, PRINT_HIGH, "No vote in progress.\n");
        return;
    }
    if (!MAY_VOTE(ent->client)) {
        gi.cprintf(ent, PRINT_HIGH, "Spectators can't vote on this server.\n");
        return;
    }
    if (ent->client->level.vote.index == level.vote.index) {
        if (ent->client->level.vote.accepted == accepted) {
            gi.cprintf(ent, PRINT_HIGH, "You have already voted %s.\n",
                       accepted ? "YES" : "NO");
            return;
        }
        if (!VF(CHANGE)) {
            gi.cprintf(ent, PRINT_HIGH, "You can't change your vote.\n");
            return;
        }
        if (VF(ANNOUNCE)) {
            gi.bprintf(PRINT_HIGH, "%s changed his vote to %s.\n",
                       ent->client->pers.netname, accepted ? "YES" : "NO");
        }
    } else {
        if (VF(ANNOUNCE)) {
            gi.bprintf(PRINT_HIGH, "%s voted %s.\n",
                       ent->client->pers.netname, accepted ? "YES" : "NO");
        }
    }

    ent->client->level.vote.index = level.vote.index;
    ent->client->level.vote.accepted = accepted;

    if (G_CheckVote()) {
        return;
    }

    if (!VF(ANNOUNCE)) {
        gi.cprintf(ent, PRINT_HIGH, "Vote cast.\n");
    }
}

static qboolean vote_victim(edict_t *ent)
{
    edict_t *other = G_SetVictim(ent, 1);
    if (!other) {
        return qfalse;
    }

    level.vote.victim = other->client;
    return qtrue;
}

static qboolean vote_map(edict_t *ent)
{
    char *name = gi.argv(2);
    map_entry_t *map;

    map = G_FindMap(name);
    if (!map) {
        gi.cprintf(ent, PRINT_HIGH, "Map '%s' is not available on this server.\n", name);
        return qfalse;
    }

    if (map->flags & MAP_NOVOTE) {
        gi.cprintf(ent, PRINT_HIGH, "Map '%s' is not available for voting.\n", map->name);
        return qfalse;
    }

    if (!strcmp(level.mapname, map->name)) {
        gi.cprintf(ent, PRINT_HIGH, "You are already playing '%s'.\n", map->name);
        return qfalse;
    }

    strcpy(level.vote.map, map->name);
    return qtrue;
}

static qboolean vote_telemode(edict_t *ent)
{
    char *s = gi.argv(2);
    qboolean v;

    if (!Q_stricmp(s, "normal") || !Q_stricmp(s, "freeze") || !Q_stricmp(s, "q2")) {
        v =  qfalse;
    } else if (!Q_stricmp(s, "nofreeze") || !Q_stricmp(s, "q3")) {
        v = qtrue;
    } else {
        gi.cprintf(ent, PRINT_HIGH, "Please specify one of normal/nofreeze/q2/q3.\n");
        return qfalse;
    }

    if (!!(int)g_teleporter_nofreeze->value == v) {
        gi.cprintf(ent, PRINT_HIGH, "Teleporter mode is already set to %s.\n", v ? "NOFREEZE" : "NORMAL");
        return qfalse;
    }

    level.vote.value = v;
    return qtrue;
}

typedef struct {
    const char  *name;
    int         bit;
    qboolean    (*func)(edict_t *);
} vote_proposal_t;

static const vote_proposal_t vote_proposals[] = {
    { "kick",       VOTE_KICK,          vote_victim     },
    { "mute",       VOTE_MUTE,          vote_victim     },
    { "map",        VOTE_MAP,           vote_map        },
    { "telemode",   VOTE_TELEMODE,      vote_telemode   },
    { NULL }
};

void Cmd_Vote_f(edict_t *ent)
{
    char buffer[MAX_STRING_CHARS];
    const vote_proposal_t *v;
    int mask = g_vote_mask->value;
    int limit = g_vote_limit->value;
    int argc = gi.argc();
    int votes[2], total;
    char *s;

    if (!mask) {
        gi.cprintf(ent, PRINT_HIGH, "Voting is disabled on this server.\n");
        return;
    }

    if (argc < 2) {
        if (!level.vote.proposal) {
            gi.cprintf(ent, PRINT_HIGH, "No vote in progress. Type '%s help' for usage.\n", gi.argv(0));
            return;
        }
        G_BuildProposal(buffer);
        total = _G_CalcVote(votes);
        gi.cprintf(ent, PRINT_HIGH,
                   "Proposal:   %s\n"
                   "Yes/No:     %d/%d [%d]\n"
                   "Timeout:    %d sec\n"
                   "Initiator:  %s\n",
                   buffer, votes[1], votes[0], total,
                   (level.vote.framenum - level.framenum) / HZ,
                   level.vote.initiator->pers.netname);
        return;
    }

    s = gi.argv(1);

//
// generic commands
//
    if (!strcmp(s, "help") || !strcmp(s, "h")) {
        gi.cprintf(ent, PRINT_HIGH,
                   "Usage: %s [yes/no/help/proposal] [arguments]\n"
                   "Available proposals:\n", gi.argv(0));

        if (mask & VOTE_KICK) {
            gi.cprintf(ent, PRINT_HIGH,
                       " kick <player_id>               Kick player from the server\n");
        }
        if (mask & VOTE_MUTE) {
            gi.cprintf(ent, PRINT_HIGH,
                       " mute <player_id>               Disallow player to talk\n");
        }
        if (mask & VOTE_MAP) {
            gi.cprintf(ent, PRINT_HIGH,
                       " map <name>                     Change current map\n");
        }
        if (mask & VOTE_TELEMODE) {
            gi.cprintf(ent, PRINT_HIGH,
                       " telemode <normal/nofreeze>     Change teleporter mode\n");
        }
        gi.cprintf(ent, PRINT_HIGH,
                   "Available commands:\n"
                   " yes/no                         Accept/deny current vote\n"
                   " help                           Show this help\n");
        return;
    }
    if (!strcmp(s, "yes") || !strcmp(s, "y")) {
        Cmd_CastVote_f(ent, qtrue);
        return;
    }
    if (!strcmp(s, "no") || !strcmp(s, "n")) {
        Cmd_CastVote_f(ent, qfalse);
        return;
    }

//
// proposals
//
    if (level.vote.proposal) {
        gi.cprintf(ent, PRINT_HIGH, "Vote is already in progress.\n");
        return;
    }

    if (!MAY_VOTE(ent->client)) {
        gi.cprintf(ent, PRINT_HIGH, "Spectators can't vote on this server.\n");
        return;
    }

    if (level.intermission_framenum) {
        gi.cprintf(ent, PRINT_HIGH, "You may not initiate votes during intermission.\n");
        return;
    }

    if (level.framenum - level.vote.framenum < 2 * HZ) {
        gi.cprintf(ent, PRINT_HIGH, "You may not initiate votes too soon.\n");
        return;
    }

    if (limit > 0 && ent->client->level.vote.count >= limit) {
        gi.cprintf(ent, PRINT_HIGH, "You may not initiate any more votes.\n");
        return;
    }

    for (v = vote_proposals; v->name; v++) {
        if (!strcmp(s, v->name)) {
            break;
        }
    }
    if (!v->name) {
        gi.cprintf(ent, PRINT_HIGH, "Unknown proposal '%s'. Type '%s help' for usage.\n", s, gi.argv(0));
        return;
    }

    if (!(mask & v->bit)) {
        gi.cprintf(ent, PRINT_HIGH, "Voting for '%s' is not allowed on this server.\n", v->name);
        return;
    }

    if (argc < 3) {
        if (v->bit == VOTE_MAP) {
            map_entry_t *map;
            char buffer[MAX_STRING_CHARS];
            size_t total, len;

            total = 0;
            LIST_FOR_EACH(map_entry_t, map, &g_map_list, list) {
                if (map->flags & MAP_NOVOTE) {
                    continue;
                }
                len = strlen(map->name);
                if (total + len + 2 >= sizeof(buffer)) {
                    break;
                }
                memcpy(buffer + total, map->name, len);
                buffer[total + len    ] = ',';
                buffer[total + len + 1] = ' ';
                total += len + 2;
            }
            if (total > 2) {
                total -= 2;
            }
            buffer[total] = 0;
            gi.cprintf(ent, PRINT_HIGH, "Available maplist: %s\n", buffer);
        } else {
            gi.cprintf(ent, PRINT_HIGH, "Argument required for '%s'. Type '%s help' for usage.\n", v->name, gi.argv(0));
        }
        return;
    }

    if (!v->func(ent)) {
        return;
    }

    level.vote.initiator = ent->client;
    level.vote.proposal = v->bit;
    level.vote.framenum = level.framenum + g_vote_time->value * HZ;
    level.vote.index++;

    G_BuildProposal(buffer);
    gi.bprintf(PRINT_HIGH, "%s has initiated a vote: %s\n",
               ent->client->pers.netname, buffer);
    ent->client->level.vote.index = level.vote.index;
    ent->client->level.vote.accepted = qtrue;
    ent->client->level.vote.count++;

    // decide vote immediately
    if (!G_CheckVote()) {
        if (VF(SHOW)) {
            gi.configstring(CS_VOTE_PROPOSAL, va("Vote: %s", buffer));
            G_UpdateVote();
        }
        gi.bprintf(PRINT_MEDIUM, "Type 'yes' in the console to accept or 'no' to deny.\n");
    }
}


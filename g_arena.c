/*
 Copyright (C) 2017 Packetflinger.com

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

/**
 * convert normal ascii string to console characters
 */
void G_AsciiToConsole(char *out, char *in) {
    uint32_t i;
    for (i=0; in[i] != '\0'; i++) {
        out[i] = (char)(in[i] | 0x80);
    }
    out[i] = '\0';
}

/**
 * Find the index in the client array for the client's current arena
 */
static int arena_find_cl_index(gclient_t *cl) {
    int i;
    if (!cl) {
        return -1;
    }
    for (i = 0; i < cl->pers.arena->client_count; i++) {
        if (cl->pers.arena->clients[i] == cl->edict) {
            return i;
        }
    }
    return -1;
}

/**
 * Find the index in the spectator array for the current arena
 */
static int arena_find_sp_index(edict_t *ent) {
    int i;
    if (!ent) {
        return -1;
    }
    for (i = 0; i < ARENA(ent)->spectator_count; i++) {
        if (ARENA(ent)->spectators[i] == ent) {
            return i;
        }
    }
    return -1;
}

/**
 * find the next open slot in the clients array
 */
static int arena_find_cl_slot(arena_t *a) {
    int i;
    if (!a) {
        return -1;
    }
    for (i = 0; i < game.maxclients; i++) {
        if (!a->clients[i]) {
            return i;
        }
    }
    return -1;
}

/**
 * find the next open slot in the spectator array
 */
static int arena_find_sp_slot(arena_t *a) {
    int i;
    if (!a) {
        return 0;
    }
    for (i = 0; i < game.maxclients; i++) {
        if (!a->spectators[i]) {
            return i;
        }
    }
    return 0;
}

/**
 * Join the spectators "team" for the player's arena
 */
void G_SpectatorsJoin(edict_t *ent) {
    int8_t idx;
    if (!ent) {
        return;
    }
    if (!ent->client) {
        return;
    }
    if (arena_find_sp_index(ent) == -1) {
        idx = arena_find_sp_slot(ARENA(ent));
        ARENA(ent)->spectators[idx] = ent;
        ARENA(ent)->spectator_count++;

        ent->movetype = MOVETYPE_NOCLIP;
        ent->solid = SOLID_NOT;
        ent->svflags |= SVF_NOCLIENT;
        ent->client->ps.gunindex = 0;
        ent->client->ps.pmove.pm_type = PM_SPECTATOR;
        ent->client->pers.connected = CONN_SPECTATOR;

        memset(&ent->client->resp, 0, sizeof(ent->client->resp));

        gi.WriteByte(SVC_TEMP_ENTITY);
        gi.WriteByte(TE_BLOOD);
        gi.WritePosition(ent->s.origin);
        gi.WriteDir(vec3_origin);
        gi.multicast(ent->s.origin, MULTICAST_PVS);

        G_SendStatusBar(ent);
    }
}

/**
 * Leave the spectator "team" for the player's arena
 */
void G_SpectatorsPart(edict_t *ent) {
    int8_t idx;
    if (!ent) {
        return;
    }
    if (!ent->client) {
        return;
    }
    idx = arena_find_sp_index(ent);
    if (idx >= 0) {
        ARENA(ent)->spectators[idx] = NULL;
        ARENA(ent)->spectator_count--;
    }
}

/**
 * Get a pointer to the team of type in an arena
 */
static arena_team_t *FindTeam(arena_t *a, arena_team_type_t type) {
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
 * Generate a name for auto-recorded demos, replace weird chars with
 * underscores
 */
const char *DemoName(edict_t *ent) {
    if (!ent) {
        return "demo";
    }
    if (!TEAM(ent)) {
        return "demo";
    }
    static char name[MAX_STRING_CHARS];
    int32_t i;
    size_t namesize;
    struct tm *ts;
    time_t t;
    cvar_t *hostname;
    cvar_t *demohostname;

    hostname = gi.cvar("hostname", "unnamed_server", 0);
    demohostname = gi.cvar("g_demo_hostname", hostname->string, 0);

    t = time(NULL);
    ts = localtime(&t);

    // server-map-arena-team-date-time
    namesize = Q_snprintf(name, sizeof(name), "%s-%s-%s-%s-%d%02d%02d-%02d%02d%02d",
        demohostname->string,
        level.mapname,
        ARENA(ent)->name,
        TEAM(ent)->name,
        ts->tm_year + 1900,
        ts->tm_mon + 1,
        ts->tm_mday,
        ts->tm_hour,
        ts->tm_min,
        ts->tm_sec
    );
    for (i = 0; i < namesize; i++) {
        if ((name[i] < '!' && name[i] > '~') || name[i] == '\\' || name[i] == '\"' ||
            name[i] == ':' || name[i] == '*' || name[i] == '/' || name[i] == '?' ||
            name[i] == '>' || name[i] == '<' || name[i] == '|' || name[i] == ' ') {
            name[i] = '_';
        }
    }
    return name;
}

/**
 * Periodically count players to make sure none got lost
 */
void update_playercounts(arena_t *a) {
    int i;
    int count = 0;
    gclient_t *cl;
    if (!a) {
        return;
    }

    for (i = 0; i < game.maxclients; i++) {
        cl = &game.clients[i];
        if (cl && cl->pers.arena == a) {
            count++;
        }
    }
    a->player_count = count;
}

/**
 * Send a scoreboard to a particular player
 */
void G_ArenaScoreboardMessage(edict_t *ent, qboolean reliable) {
    char buffer[MAX_STRING_CHARS];
    if (!ent) {
        return;
    }
    if (ARENA(ent)->state == ARENA_STATE_WARMUP) {
        G_BuildPregameScoreboard(buffer, ent->client, ARENA(ent));
    } else {
        G_BuildScoreboard(buffer, ent->client, ARENA(ent));
    }
    gi.WriteByte(SVC_LAYOUT);
    gi.WriteString(buffer);
    gi.unicast(ent, reliable);
}

/**
 * Playerboard lists who is in what arena
 */
void G_ArenaPlayerboardMessage(edict_t *ent, qboolean reliable) {
    char buffer[MAX_STRING_CHARS];
    if (!ent) {
        return;
    }
    G_BuildPlayerboard(buffer, ARENA(ent));
    gi.WriteByte(SVC_LAYOUT);
    gi.WriteString(buffer);
    gi.unicast(ent, reliable);
}

/**
 * start a sound for all arena members, people in other arenas shouldn't hear it
 */
void G_ArenaSound(arena_t *a, int index) {
    if (!a) {
        return;
    }
    clamp(index, 318, 574); // CS_SOUNDS through CS_IMAGES
    int i;
    gclient_t *cl;

    for (i = 0; i < game.maxclients; i++) {
        cl = &game.clients[i];
        if (!cl) {
            continue;
        }
        if (cl->pers.arena == a) {
            gi.sound(cl->edict, CHAN_AUTO, index, 1, ATTN_NORM, 0);
        }
    }
}

/**
 * Stuff a command to each client in an arena
 */
void G_ArenaStuff(arena_t *a, const char *command) {
    if (!a) {
        return;
    }
    if (*command == 0) {
        gi.dprintf("%s(): empty command argument\n", __func__);
        return;
    }
    int i;
    gclient_t *cl;

    for (i = 0; i < game.maxclients; i++) {
        cl = &game.clients[i];
        if (!cl) {
            continue;
        }
        if (cl->pers.arena == a) {
            gi.WriteByte(SVC_STUFFTEXT);
            gi.WriteString(command);
            gi.unicast(cl->edict, qtrue);
        }
    }
}


/**
 * Check for things like players dropping from teams
 */
void G_CheckArenaRules(arena_t *a) {
    uint8_t i;

    if (!a) {
        return;
    }
    // watch for players disconnecting mid match
    if (MATCHPLAYING(a)) {
        for (i=0; i<a->team_count; i++) {
            if (a->teams[i].player_count == 0) {
                G_EndMatch(a, NULL);
                return;
            }
        }
    }
}

/**
 * Look for and handle stage changes.
 * Match starts are handled via countdown clock callback
 * EOM (via timeout) handled via match clock callback
 */
void G_CheckState(arena_t *a) {
    if (!a) {
        return;
    }
    if (a->state == ARENA_STATE_PLAY && a->teams_alive == 1) { //round finished
        G_BeginRoundIntermission(a);
    }
    if (a->state == ARENA_STATE_WARMUP && a->ready) { // everyone ready, start
        a->state = ARENA_STATE_COUNTDOWN;
        a->round_start_frame = level.framenum
                + SECS_TO_FRAMES((int) g_round_countdown->value);
        a->round_frame = a->round_start_frame;
        a->match_frame = a->round_start_frame;
        a->countdown = (int) g_round_countdown->value;

        G_ClearRoundInfo(a);
        G_RespawnPlayers(a);
        G_ForceDemo(a);
        ClockStartRoundCountdown(a);
    }
}

/**
 * Run once per frame for each arena in the map. Keeps track of state changes,
 * round/match beginning and ending, timeouts, countdowns, votes, etc.
 *
 */
void G_ArenaThink(arena_t *a) {
    if (!a) {
        return;
    }

    if (a->client_count == 0) {
        return;
    }

    if (a->state == ARENA_STATE_TIMEOUT) {
        if (a->timeout_clock.tick) {
            a->timeout_clock.tick(&a->timeout_clock);
        }
        return;
    }

    G_CheckState(a);
    G_CheckVoteStatus(a);
    G_CheckArenaRules(a);

    if (a->state > ARENA_STATE_WARMUP) {
        a->round_frame++;
        a->match_frame++;
    }

    if (a->clock.tick) {
        a->clock.tick(&a->clock);
    }
}

/**
 * Stuff that needs to be reset between rounds
 */
void G_ClearRoundInfo(arena_t *a) {
    uint8_t i;
    if (!a) {
        gi.dprintf("%s(): null arena\n", __func__);
        return;
    }
    a->current_round = 1;
    G_ConfigString(a, CS_ROUND, G_RoundToString(a));
    for (i=0; i<a->team_count; i++) {
        a->teams[i].damage_dealt = 0;
        a->teams[i].damage_taken = 0;
    }
    a->teams_alive = a->team_count;
}

/**
 * Send a configstring to everyone in a particular arena
 */
void G_ConfigString(arena_t *arena, uint16_t index, const char *string) {
    uint8_t i;
    edict_t *ent;
    if (!arena) {
        gi.dprintf("%s(): null arena\n", __func__);
        return;
    }
    clamp(index, 0, 2080); // CS_MAX

    for (i=0; i<game.maxclients; i++) {
        ent = arena->clients[i];
        if (!ent) {
            continue;
        }
        if (!ent->client) {
            continue;
        }
        gi.WriteByte(SVC_CONFIGSTRING);
        gi.WriteShort(index);
        gi.WriteString(string);
        gi.unicast(ent, qtrue);
    }
}

/**
 * Broadcast print to only members of specified arena
 */
void G_bprintf(arena_t *arena, int level, const char *fmt, ...) {
    va_list argptr;
    char string[MAX_STRING_CHARS];
    size_t len;
    int i;
    edict_t *other;
    if (!arena) {
        gi.dprintf("%s(): null arena\n", __func__);
        return;
    }
    clamp(level, PRINT_LOW, PRINT_CHAT);
    va_start(argptr, fmt);
    len = Q_vsnprintf(string, sizeof(string), fmt, argptr);
    va_end(argptr);

    if (len >= sizeof(string)) {
        return;
    }
    for (i = 1; i <= game.maxclients; i++) {
        other = &g_edicts[i];
        if (!other->inuse) {
            continue;
        }
        if (!other->client) {
            continue;
        }
        if (arena != ARENA(other)) {
            continue;
        }
        gi.cprintf(other, level, "%s", string);
    }
}

/**
 * Build the arena menu structure
 */
void G_BuildMenu(void) {
    int i, j;

    memset(&menu_lookup, 0, sizeof(pmenu_arena_t) * MAX_ARENAS);
    for (i = 0, j = 0; i < MAX_ARENAS; i++) {
        if (level.arenas[i].number) {
            menu_lookup[j].num = level.arenas[i].number;
            Q_strlcpy(menu_lookup[j].name, level.arenas[i].name,
                    sizeof(menu_lookup[j].name));
            j++;
        }
    }
}

/**
 * Used to update per-client scoreboard and build
 * global oldscores (client is NULL in the latter case).
 *
 * Build Vertically
 */
size_t G_BuildScoreboard(char *buffer, gclient_t *client, arena_t *arena) {
    char entry[MAX_STRING_CHARS];
    char status[MAX_QPATH];
    char timebuf[16];
    size_t total, len;
    int i, j, k, numranks;
    int y, sec;
    gclient_t *ranks[MAX_CLIENTS];
    gclient_t *c;
    time_t t;
    struct tm *tm;
    edict_t *ent;

    if (!buffer) {
        gi.dprintf("%s(): null buffer\n", __func__);
        return 0;
    }
    if (!arena) {
        gi.dprintf("%s(): null arena\n", __func__);
        return 0;
    }

    // starting point down from top of screen
    y = 20;

    // Build time string
    t = time(NULL);
    tm = localtime(&t);
    len = strftime(status, sizeof(status), DATE_FORMAT, tm);

    if (len < 1) {
        strcpy(status, "???");
    }

    if (!client) {
        Q_snprintf(entry, sizeof(entry),
                "yt %d cstring2 \"Old scoreboard from %s on %s\"", y, arena->name, status);
    } else {
        Q_snprintf(entry, sizeof(entry), "yt %d cstring2 \"%s - %s\"", y,
                status, arena->name);
    }

    // move down 6 lines
    y += LAYOUT_LINE_HEIGHT * 6;

    total = 0;
    for (k=0; k<arena->team_count; k++) {
        total += Q_scnprintf(buffer + total, MAX_STRING_CHARS, "xv 0 %s "
                "yt %d "
                "cstring \"Team %s - %d\" "
                "yt %d "
                "cstring2 \"Name                 Score      Time Ping\"", entry, y,
                arena->teams[k].name,
                arena->teams[k].points,
                y + LAYOUT_LINE_HEIGHT
        );

        numranks = G_CalcArenaRanks(ranks, &arena->teams[k]);

        // hometeam first, add the clients sorted by rank
        y += LAYOUT_LINE_HEIGHT * 2;
        for (i = 0; i < numranks; i++) {
            c = ranks[i];

            sec = (level.framenum - c->resp.enter_framenum) * FRAMETIME;

            if (!sec) {
                sec = 1;
            }

            if (c->pers.score > 0) {
                j = c->pers.score + c->resp.deaths;
            }

            if (level.framenum < 10 * 60 * HZ) {
                sprintf(timebuf, "%d:%02d", sec / 60, sec % 60);
            } else {
                sprintf(timebuf, "%d", sec / 60);
            }

            len = Q_snprintf(entry, sizeof(entry),
                    "yt %d cstring \"%-21s %6d     %4s %4d\"", y,
                    c->pers.netname,
                    c->pers.score,
                    timebuf,
                    c->ping
            );

            if (len >= sizeof(entry)) {
                continue;
            }

            if (total + len >= MAX_STRING_CHARS) {
                break;
            }

            memcpy(buffer + total, entry, len);
            total += len;
            y += LAYOUT_LINE_HEIGHT;
        }

        // skip 4 lines
        y += LAYOUT_LINE_HEIGHT * 4;

    }

    // add spectators in fixed order
    total += Q_scnprintf(buffer + total, MAX_STRING_CHARS,
            "yt %d cstring2 \"---- Spectators ----\"", y);
    y += LAYOUT_LINE_HEIGHT;

    for (i = 0; i < arena->spectator_count; i++) {
        ent = arena->spectators[i];
        if (!ent) {
            continue;
        }

        if (!ent->client) {
            continue;
        }

        if (ent->client->pers.mvdspec) {
            continue;
        }

        // spec is following someone, show who
        if (ent->client->chase_target) {
            Q_snprintf(status, sizeof(status), "-> %.13s",
                    ent->client->chase_target->client->pers.netname);
        } else {
            status[0] = 0;
        }

        len = Q_snprintf(entry, sizeof(entry),
                "yt %d cstring \"%s\"", y,
                va("%s:%d %s", NAME(ent), ent->client->ping, status));

        if (len >= sizeof(entry)) {
            continue;
        }

        if (total + len >= MAX_STRING_CHARS) {
            break;
        }

        memcpy(buffer + total, entry, len);

        total += len;
        y += LAYOUT_LINE_HEIGHT;
        j++;
    }

    // add server info
    if (sv_hostname && sv_hostname->string[0]) {
        len = Q_scnprintf(entry, sizeof(entry), "xl 8 yb -45 string2 \"%s\"",
                sv_hostname->string
        );

        if (total + len < MAX_STRING_CHARS) {
            memcpy(buffer + total, entry, len);
            total += len;
        }
    }
    buffer[total] = 0;
    return total;
}

/**
 * Scoreboard before match starts
 */
size_t G_BuildPregameScoreboard(char *buffer, gclient_t *client, arena_t *arena) {
    char entry[MAX_STRING_CHARS];
    char status[MAX_QPATH];
    char timebuf[16];
    size_t total, len;
    int i, j = 0, k, numranks;
    int y, sec;
    gclient_t *ranks[MAX_CLIENTS];
    gclient_t *c;
    time_t t;
    struct tm *tm;
    edict_t *ent;

    if (!buffer) {
        gi.dprintf("%s(): null buffer\n", __func__);
        return 0;
    }
    if (!arena) {
        gi.dprintf("%s(): null arena\n", __func__);
        return 0;
    }

    char bracketopen[2];
    char bracketclosed[2];
    G_AsciiToConsole(bracketopen, "[");
    G_AsciiToConsole(bracketclosed, "]");
    
    // starting point down from top of screen
    y = 20;

    // Build time string
    t = time(NULL);
    tm = localtime(&t);
    len = strftime(status, sizeof(status), DATE_FORMAT, tm);

    if (len < 1) {
        strcpy(status, "???");
    }

    if (!client) {
        Q_snprintf(entry, sizeof(entry),
                "yt %d cstring2 \"Old scoreboard from %s on %s\"", y, arena->name, status);
    } else {
        Q_snprintf(entry, sizeof(entry), "yt %d cstring2 \"%s - %s\"", y,
                status, arena->name);
    }

    // move down 6 lines
    y += LAYOUT_LINE_HEIGHT * 6;

    total = 0;

    // loop through teams
    for (k=0; k<arena->team_count; k++) {

        total += Q_scnprintf(buffer + total, MAX_STRING_CHARS, "xv 0 %s "
                "yt %d "
                "cstring \"Team %s\" "
                "yt %d "
                "cstring2 \"Name                            Time Ping\" ", entry, y,
                arena->teams[k].name, y + LAYOUT_LINE_HEIGHT);

        numranks = G_CalcArenaRanks(ranks, &arena->teams[k]);

        // hometeam first, add the clients sorted by rank
        y += LAYOUT_LINE_HEIGHT * 2;

        for (i = 0; i < numranks; i++) {
            c = ranks[i];

            sec = (level.framenum - c->resp.enter_framenum) * FRAMETIME;

            if (!sec) {
                sec = 1;
            }

            if (c->resp.score > 0) {
                j = c->resp.score + c->resp.deaths;
            }

            if (level.framenum < 10 * 60 * HZ) {
                sprintf(timebuf, "%d:%02d", sec / 60, sec % 60);
            } else {
                sprintf(timebuf, "%d", sec / 60);
            }

            len = Q_snprintf(entry, sizeof(entry),
                    "yt %d cstring \"%-15s %-15s %4s %4d\"", y, c->pers.netname,
                    (c->pers.ready) ? va("%sready%s", bracketopen, bracketclosed) : "",
                    timebuf, c->ping);

            if (len >= sizeof(entry)) {
                continue;
            }

            if (total + len >= MAX_STRING_CHARS) {
                break;
            }

            memcpy(buffer + total, entry, len);

            total += len;
            y += LAYOUT_LINE_HEIGHT;
        }

        // skip 4 lines
        y += LAYOUT_LINE_HEIGHT * 4;
    }

    // add spectators in fixed order
    total += Q_scnprintf(buffer + total, MAX_STRING_CHARS,
            "yt %d cstring2 \"---- Spectators ----\"", y);
    y += LAYOUT_LINE_HEIGHT;

    for (i = 0; i < arena->spectator_count; i++) {
        ent = arena->spectators[i];
        if (!ent) {
            continue;
        }

        if (!ent->client) {
            continue;
        }

        if (ent->client->pers.mvdspec) {
            continue;
        }

        // spec is following someone, show who
        if (ent->client->chase_target) {
            Q_snprintf(status, sizeof(status), "-> %.13s",
                    ent->client->chase_target->client->pers.netname);
        } else {
            status[0] = 0;
        }

        len = Q_snprintf(entry, sizeof(entry),
                "yt %d cstring \"%s\"", y,
                va("%s:%d %s", NAME(ent), ent->client->ping, status));

        if (len >= sizeof(entry)) {
            continue;
        }

        if (total + len >= MAX_STRING_CHARS) {
            break;
        }

        memcpy(buffer + total, entry, len);

        total += len;
        y += LAYOUT_LINE_HEIGHT;
        j++;
    }

    // add server info
    if (sv_hostname && sv_hostname->string[0]) {
        len = Q_scnprintf(entry, sizeof(entry), "xl 8 yb -45 string2 \"%s\"",
                sv_hostname->string
        );

        if (total + len < MAX_STRING_CHARS) {
            memcpy(buffer + total, entry, len);
            total += len;
        }
    }
    buffer[total] = 0;
    return total;
}

/**
 * Show all players connected.
 */
size_t G_BuildPlayerboard(char *buffer, arena_t *arena) {
    char entry[MAX_STRING_CHARS], status[MAX_QPATH];
    size_t total, len;
    int i, y;
    gclient_t *c;
    time_t t;
    arena_t *a;

    if (!buffer) {
        gi.dprintf("%s(): null buffer\n", __func__);
        return 0;
    }
    if (!arena) {
        gi.dprintf("%s(): null arena\n", __func__);
        return 0;
    }

    // starting point down from top of screen
    y = 20;

    // Build time string
    t = time(NULL);
    len = strftime(status, sizeof(status), DATE_FORMAT, (struct tm *) localtime(&t));

    if (len < 1) {
        strcpy(status, "Beer O'Clock");
    }

    Q_snprintf(entry, sizeof(entry), "yt %d cstring2 \"%s - %s\"", y,
            status, arena->name);

    // move down 6 lines
    y += LAYOUT_LINE_HEIGHT * 6;

    total = Q_scnprintf(buffer, MAX_STRING_CHARS, "xv 0 %s "
            "yt %d "
            "cstring \"Connected Players\" "
            "yt %d "
            "cstring2 \" Name           Arena                Ping\" ", entry, y,
             y + LAYOUT_LINE_HEIGHT * 2);

    y += LAYOUT_LINE_HEIGHT * 4;

    for (i = 0; i < game.maxclients; i++) {
        c = &game.clients[i];

        if (!c || c->pers.connected <= CONN_CONNECTED) {
            continue;
        }

        a = c->pers.arena;

        len = Q_snprintf(entry, sizeof(entry),
                "yt %d cstring \"%-15s %-20s %4d\"", y,
                c->pers.netname, va("%d:%s", a->number, a->name), c->ping);

        if (len >= sizeof(entry)) {
            continue;
        }

        if (total + len >= MAX_STRING_CHARS) {
            break;
        }

        memcpy(buffer + total, entry, len);

        total += len;
        y += LAYOUT_LINE_HEIGHT;
    }

    // add server info
    if (sv_hostname && sv_hostname->string[0]) {
        len = Q_scnprintf(entry, sizeof(entry), "xl 8 yb -45 string2 \"%s\"",
                sv_hostname->string
        );

        if (total + len < MAX_STRING_CHARS) {
            memcpy(buffer + total, entry, len);
            total += len;
        }
    }
    buffer[total] = 0;
    return total;
}

/**
 * Rank players by their scores
 */
int G_CalcArenaRanks(gclient_t **ranks, arena_team_t *team) {
    int i, total;

    if (!team) {
        gi.dprintf("%s(): null team\n", __func__);
        return 0;
    }

    // sort the clients by score, then by eff
    total = 0;
    for (i = 0; i < MAX_TEAM_PLAYERS; i++) {
        if (!team->players[i]) {
            continue;
        }
        if (team->players[i]->client->pers.connected == CONN_SPAWNED) {
            if (ranks) {
                ranks[total] = team->players[i]->client;
            }
            total++;
        }
    }
    if (ranks) {
        qsort(ranks, total, sizeof(gclient_t *), G_PlayerCmp);
    }
    return total;
}

/**
 * Send a centerprint to every player of an arena
 */
void G_Centerprintf(arena_t *a, const char *fmt, ...) {
    va_list argptr;
    char string[MAX_STRING_CHARS];
    size_t len;
    int i, j;
    edict_t *ent;
    arena_team_t *team;

    if (!a) {
        gi.dprintf("%s(): null arena\n", __func__);
        return;
    }

    va_start(argptr, fmt);
    len = Q_vsnprintf(string, sizeof(string), fmt, argptr);
    va_end(argptr);
    if (len == 0) {
        return;
    }
    for (i=0; i<a->team_count; i++) {
        team = &a->teams[i];
        for (j=0; j<MAX_TEAM_PLAYERS; j++) {
            ent = team->players[j];
            if (ent && ent->inuse) {
                gi.WriteByte(SVC_CENTERPRINT);
                gi.WriteString(string);
                gi.unicast(ent, true);
            }
        }
    }
}

/**
 * Move a client to a different arena.
 *
 * cl - the client moving
 * arena - the new (destination) arena, can be null (part only)
 */
void G_ChangeArena(edict_t *ent, arena_t *arena) {
    int index = 0, i;
    char roundtime[6];

    if (!ent) {
        return;
    }
    if (!ent->client) {
        return;
    }
    // leave the old arena
    if (ARENA(ent)) {
        index = arena_find_cl_index(ent->client);
        ARENA(ent)->clients[index] = NULL;
        ARENA(ent)->client_count--;
        G_TeamPart(ent, true);
        if (arena) {
            for (i=0; i<ARENA(ent)->client_count; i++) {
                if (!ARENA(ent)->clients[i]) {
                    continue;
                }
                if (ARENA(ent)->clients[i] == ent) {
                    continue;   // don't bother sending to the one moving
                }
                gi.cprintf(ARENA(ent)->clients[i], PRINT_HIGH, "%s left this arena\n", NAME(ent));
            }
        }
        G_SpectatorsPart(ent);
    }

    if (!arena) {
        return;
    }
    index = arena_find_cl_slot(arena);
    // reset arena back to defaults
    if (arena->modified && arena->client_count == 0) {
        G_ApplyDefaults(arena);
    }
    arena->client_count++;
    arena->clients[index] = ent;
    ARENA(ent) = arena;
    ent->client->pers.connected = CONN_SPECTATOR;
    ent->client->pers.ready = false;
    G_SpectatorsJoin(ent);
    ClientString(ent, CS_ROUND, G_RoundToString(ARENA(ent)));
    G_SecsToString(roundtime, arena->timelimit * 60);
    ClientString(ent, CS_MATCH_STATUS, va("Warmup %s", roundtime));
    G_UpdateSkins(ent); // send all current player skins to this new player
    G_MovePlayerToSpawnSpot(ent, G_SpawnPoint(ent));
    G_ArenaSound(arena, level.sounds.teleport);
    G_bprintf(arena, PRINT_HIGH, "%s joined this arena\n", NAME(ent));

    // hold in place briefly
    ent->client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
    ent->client->ps.pmove.pm_time = 14;
    ent->client->respawn_framenum = level.framenum;
}

/**
 * Check if all players are ready and set the next time we should check
 */
void G_CheckReady(arena_t *a) {
    uint8_t i, count;
    if (!a) {
        gi.dprintf("%s(): null arena\n", __func__);
        return;
    }
    count = 0;
    for (i=0; i<a->team_count; i++) {
        // all teams need at least 1 player to be considered ready
        if (a->teams[i].player_count == 0)    {
            a->ready = qfalse;
            return;
        }
        // any team not currently ready means arena isn't ready
        if (!a->teams[i].ready) {
            a->ready = qfalse;
            return;
        }
        if (g_team_balance->value > 0 && a->teams[i].player_count != count && count != 0) {
            a->ready = qfalse;
            return;
        }
        count = a->teams[i].player_count;
    }
    // if we made it this far, everyone is ready, start the round
    a->ready = true;
}

/**
 * Send strings to clients depending on the state
 */
void G_UpdateConfigStrings(arena_t *a) {
    char buf[0xff];
    char roundtime[10];
    buf[0] = 0;
    if (!a) {
        gi.dprintf("%s(): null arena\n", __func__);
        return;
    }
    switch (a->state) {
    case ARENA_STATE_WARMUP:
        G_SecsToString(roundtime, a->timelimit);
        strcat(buf, va("Warmup %s", roundtime));
        break;
    case ARENA_STATE_COUNTDOWN:
        strcat(buf, va("Starting in %s", a->clock.string));
        break;
    case ARENA_STATE_PLAY:
        strcat(buf, va("Playing %s", a->clock.string));
        break;
    case ARENA_STATE_TIMEOUT:
        strcat(buf, va("Timeout %s   (%s)", a->timeout_clock.string, a->clock.string));
        break;
    case ARENA_STATE_RINTERMISSION:
    case ARENA_STATE_MINTERMISSION:
        strcat(buf, va("Intermission %s", a->clock.string));
        break;
    default:
        buf[0] = 0;
    }
    if (buf[0]) {
        G_ConfigString(a, CS_MATCH_STATUS, buf);
    }
}

/**
 * Send this edict the skins everyone is using so they display properly
 */
void G_UpdateSkins(edict_t *ent) {
    uint8_t i, j;

    if (!ent) {
        gi.dprintf("%s(): null edict\n", __func__);
        return;
    }
    // each team
    for (i=0; i<ARENA(ent)->team_count; i++) {
        // each player
        for (j=0; j<ARENA(ent)->teams[i].player_count; j++) {
            gi.WriteByte(SVC_CONFIGSTRING);
            gi.WriteShort(CS_PLAYERSKINS + (ARENA(ent)->teams[i].players[j] - g_edicts) - 1);
            gi.WriteString(ARENA(ent)->teams[i].skin);
            gi.unicast(ent, qtrue);
        }
    }
}

/**
 * Force the ready state for all players on a team
 */
void G_ForceReady(arena_team_t *team, qboolean ready) {
    int i;
    if (!team) {
        gi.dprintf("%s(): null team\n", __func__);
        return;
    }
    for (i = 0; i < MAX_TEAM_PLAYERS; i++) {
        if (team->players[i]) {
            team->players[i]->client->pers.ready = ready;
        }
    }
    team->ready = ready;
}

/**
 * Make all players in an arena record a demo of the match
 */
void G_ForceDemo(arena_t *arena) {
    uint8_t i, j;
    edict_t *ent;
    arena_team_t *team;

    if (!arena) {
        gi.dprintf("%s(): null arena\n", __func__);
        return;
    }
    if (!g_demo->value) {
        return;
    }
    if (arena->recording) {
        for (i=0; i<arena->team_count; i++) {
            team = &arena->teams[i];
            for (j=0; j<MAX_TEAM_PLAYERS; j++) {
                ent = team->players[j];
                if (ent && ent->inuse) {
                    G_StuffText(ent, "stop\n");
                }
            }
        }
        arena->recording = qfalse;
    } else {
        for (i=0; i<arena->team_count; i++) {
            team = &arena->teams[i];
            for (j=0; j<MAX_TEAM_PLAYERS; j++) {
                ent = team->players[j];
                if (ent && ent->inuse) {
                    G_StuffText(ent, va("record \"%s\"\n", DemoName(ent)));
                }
            }
        }
        arena->recording = qtrue;
    }
}

/**
 * Make all players take a screen of the match intermission screen
 */
void G_ForceScreenshot(arena_t *arena) {
    uint8_t i, j;
    edict_t *ent;
    arena_team_t *team;

    if (!arena) {
        gi.dprintf("%s(): null arena\n", __func__);
        return;
    }
    if (!g_screenshot->value) {
        return;
    }
    for (i=0; i<arena->team_count; i++) {
        team = &arena->teams[i];
        for (j=0; j<MAX_TEAM_PLAYERS; j++) {
            ent = team->players[j];
            if (ent && ent->inuse) {
                G_StuffText(ent, "wait; screenshot\n");
            }
        }
    }
}

/**
 * Reset after match is finished
 */
void G_EndMatch(arena_t *a, arena_team_t *winner) {
    uint8_t i;

    if (!a) {
        gi.dprintf("%s(): null arena\n", __func__);
        return;
    }
    if (winner) {
        for (i = 0; i < MAX_TEAM_PLAYERS; i++) {
            if (!winner->players[i]) {
                continue;
            }
            winner->players[i]->client->resp.match_score++;
        }
    }

    // save the current scoreboard as an oldscore
    G_BuildScoreboard(a->oldscores, NULL, a);
    G_bprintf(a, PRINT_HIGH, "Match finished\n");
    G_ArenaSound(a, level.sounds.horn);
    BeginIntermission(a);
    G_ForceScreenshot(a);
    G_ForceDemo(a);
    ClockStartMatchIntermission(a);
}

/**
 * All players on a particular team have been fragged
 */
void G_EndRound(arena_t *a, arena_team_t *winner) {
    int i;
    if (!a) {
        gi.dprintf("%s(): null arena\n", __func__);
        return;
    }
    a->round_start_frame = 0;

    if (winner) {
        G_bprintf(a, PRINT_HIGH, "Team %s won round %d/%d!\n", winner->name,
                a->current_round, a->round_limit);
        update_playercounts(a);    // for sanity
        for (i = 0; i < MAX_TEAM_PLAYERS; i++) {
            if (!winner->players[i]) {
                continue;
            }
            winner->players[i]->client->resp.round_score++;
        }
    }

    if (a->current_round == a->round_limit) {
        G_EndMatch(a, winner);
        return;
    }

    a->current_round++;
    a->state = ARENA_STATE_COUNTDOWN;
    a->countdown = (int) g_round_countdown->value;
    a->round_start_frame = level.framenum + SECS_TO_FRAMES(a->countdown);
    a->teams_alive = a->team_count;

    G_ConfigString(a, CS_ROUND, G_RoundToString(a));
    G_HideScores(a);
    G_RespawnPlayers(a);
    ClockStartRoundCountdown(a);
}

/**
 * Lock players in place during timeouts
 *
 */
void G_FreezePlayers(arena_t *a, qboolean freeze) {
    uint8_t i, j;
    arena_team_t *team;

    if (!a) {
        return;
    }
    for (i=0; i<a->team_count; i++) {
        team = &a->teams[i];
        for (j=0; j<MAX_TEAM_PLAYERS; j++) {
            G_FreezePlayer(team->players[j], freeze);
        }
    }
}

/**
 * Lock a player in place (for timeouts)
 */
void G_FreezePlayer(edict_t *ent, qboolean freeze) {
    if (!ent) {
        gi.dprintf("%s(): null edict\n", __func__);
        return;
    }
    if (!ent->inuse) {
        return;
    }
    if (!ent->client) {
        return;
    }
    ent->client->ps.pmove.pm_type = (freeze) ? PM_FREEZE : PM_NORMAL;
}

/**
 * Give the player all the items/weapons they need. Called in PutClientInServer()
 *
 */
void G_GiveItems(edict_t *ent) {
    if (!ent) {
        gi.dprintf("%s(): null edict\n", __func__);
        return;
    }
    if (!ent->client) {
        return;
    }
    
    int flags;
    arena_t *a;
    a = ARENA(ent);
    
    // remove it all first
    ent->client->inventory[ITEM_SHOTGUN] = 0;
    ent->client->inventory[ITEM_SUPERSHOTGUN] = 0;
    ent->client->inventory[ITEM_MACHINEGUN] = 0;
    ent->client->inventory[ITEM_CHAINGUN] = 0;
    ent->client->inventory[ITEM_GRENADELAUNCHER] = 0;
    ent->client->inventory[ITEM_HYPERBLASTER] = 0;
    ent->client->inventory[ITEM_ROCKETLAUNCHER] = 0;
    ent->client->inventory[ITEM_RAILGUN] = 0;
    ent->client->inventory[ITEM_BFG] = 0;
    ent->client->inventory[ITEM_GRENADES] = 0;    // item & weapon
    ent->client->inventory[ITEM_SLUGS] = 0;
    ent->client->inventory[ITEM_ROCKETS] = 0;
    ent->client->inventory[ITEM_CELLS] = 0;
    ent->client->inventory[ITEM_GRENADES] = 0;
    ent->client->inventory[ITEM_BULLETS] = 0;
    ent->client->inventory[ITEM_SHELLS] = 0;


    flags = a->weapon_flags;

    if (flags < 2) {
        flags = ARENAWEAPON_ALL;
    }

    // weapons and armor
    if (flags & ARENAWEAPON_SHOTGUN) {
        ent->client->inventory[ITEM_SHOTGUN] = 1;
        ent->client->inventory[ITEM_SHELLS] = a->ammo[ITEM_SHELLS];
    }

    if (flags & ARENAWEAPON_SUPERSHOTGUN) {
        ent->client->inventory[ITEM_SUPERSHOTGUN] = 1;
        ent->client->inventory[ITEM_SHELLS] = a->ammo[ITEM_SHELLS];
    }

    if (flags & ARENAWEAPON_MACHINEGUN) {
        ent->client->inventory[ITEM_MACHINEGUN] = 1;
        ent->client->inventory[ITEM_BULLETS] = a->ammo[ITEM_BULLETS];
    }

    if (flags & ARENAWEAPON_CHAINGUN) {
        ent->client->inventory[ITEM_CHAINGUN] = 1;
        ent->client->inventory[ITEM_BULLETS] = a->ammo[ITEM_BULLETS];
    }

    if (flags & ARENAWEAPON_GRENADELAUNCHER) {
        ent->client->inventory[ITEM_GRENADELAUNCHER] = 1;
        ent->client->inventory[ITEM_GRENADES] = a->ammo[ITEM_GRENADES];
    }

    if (flags & ARENAWEAPON_HYPERBLASTER) {
        ent->client->inventory[ITEM_HYPERBLASTER] = 1;
        ent->client->inventory[ITEM_CELLS] = a->ammo[ITEM_CELLS];
    }

    if (flags & ARENAWEAPON_ROCKETLAUNCHER) {
        ent->client->inventory[ITEM_ROCKETLAUNCHER] = 1;
        ent->client->inventory[ITEM_ROCKETS] = a->ammo[ITEM_ROCKETS];
    }

    if (flags & ARENAWEAPON_RAILGUN) {
        ent->client->inventory[ITEM_RAILGUN] = 1;
        ent->client->inventory[ITEM_SLUGS] = a->ammo[ITEM_SLUGS];
    }

    if (flags & ARENAWEAPON_BFG) {
        ent->client->inventory[ITEM_BFG] = 1;
        ent->client->inventory[ITEM_CELLS] = a->ammo[ITEM_CELLS];
    }

    ent->client->inventory[ITEM_ARMOR_BODY] = a->armor;
    ent->health = a->health;
}

/**
 * Add player to a team
 */
void G_TeamJoin(edict_t *ent, arena_team_type_t type, qboolean forced) {
    if (!ent) {
        gi.dprintf("%s(): null edict\n", __func__);
        return;
    }
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

    if (team->player_count == MAX_TEAM_PLAYERS) {
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
    for (i = 0; i < MAX_TEAM_PLAYERS; i++) {
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
    G_RespawnPlayer(ent);
}

/**
 * Remove this player from whatever team they're on
 */
void G_TeamPart(edict_t *ent, qboolean silent) {
    arena_team_t *oldteam;
    int i;
    if (!ent) {
        gi.dprintf("%s(): null edict\n", __func__);
        return;
    }
    if (!ent->client) {
        return;
    }

    oldteam = TEAM(ent);

    if (!oldteam) {
        return;
    }

    // remove player
    for (i = 0; i < MAX_TEAM_PLAYERS; i++) {
        if (oldteam->players[i] == ent) {
            oldteam->players[i] = NULL;
            break;
        }
    }

    // we're the captain, reassign to next player
    if (oldteam->captain == ent) {
        for (i = 0; i < MAX_TEAM_PLAYERS; i++) {
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
    G_CheckTeamReady(oldteam);
    G_CheckArenaReady(ARENA(ent));
}

/**
 * Give back all the ammo, health and armor for start of a round
 */
void G_RefillInventory(edict_t *ent) {
    if (!ent) {
        gi.dprintf("%s(): null edict\n", __func__);
        return;
    }
    arena_t *a = ARENA(ent);

    ent->client->inventory[ITEM_SLUGS]    = a->ammo[ITEM_SLUGS];
    ent->client->inventory[ITEM_ROCKETS]  = a->ammo[ITEM_ROCKETS];
    ent->client->inventory[ITEM_CELLS]    = a->ammo[ITEM_CELLS];
    ent->client->inventory[ITEM_GRENADES] = a->ammo[ITEM_GRENADES];
    ent->client->inventory[ITEM_BULLETS]  = a->ammo[ITEM_BULLETS];
    ent->client->inventory[ITEM_SHELLS]   = a->ammo[ITEM_SHELLS];
    ent->client->inventory[ITEM_ARMOR_BODY] = a->armor;
    ent->health = a->health;
}

/**
 * refill all inventory for all players in this arena
 */
void G_RefillPlayers(arena_t *a) {
    uint8_t i, j;
    edict_t *ent;
    if (!a) {
        gi.dprintf("%s(): null arena\n", __func__);
        return;
    }
    // for each team
    for (i = 0; i < a->team_count; i++) {
        // for each player
        for (j = 0; j < MAX_TEAM_PLAYERS; j++) {
            ent = a->teams[i].players[j];

            if (ent && ent->inuse) {
                G_GiveItems(ent);
            }
        }
    }
}

/**
 * Respawn all players resetting their inventory
 */
void G_RespawnPlayers(arena_t *a) {
    uint8_t i, j;
    if (!a) {
        gi.dprintf("%s(): null arena\n", __func__);
        return;
    }
    for (i = 0; i < a->team_count; i++) {
        for (j = 0; j < MAX_TEAM_PLAYERS; j++) {
            G_RespawnPlayer(a->teams[i].players[j]);
        }
        a->teams[i].players_alive = a->teams[i].player_count;
    }
}

/**
 * Format a string saying what round we're on
 */
char *G_RoundToString(arena_t *a) {
    static char round_buffer[14];
    if (!a) {
        gi.dprintf("%s(): null arena\n", __func__);
        return round_buffer;
    }
    sprintf(round_buffer, "Round %02d/%02d", a->current_round, a->round_limit);
    return round_buffer;
}

/**
 * Display scores layout to all arena players
 */
void G_ShowScores(arena_t *a) {
    uint8_t i, j;
    edict_t *ent;
    if (!a) {
        gi.dprintf("%s(): null arena\n", __func__);
        return;
    }
    // for each team
    for (i = 0; i < a->team_count; i++) {
        // for each player
        for (j = 0; j < MAX_TEAM_PLAYERS; j++) {
            ent = a->teams[i].players[j];

            if (ent && ent->inuse) {
                G_ArenaScoreboardMessage(ent, true);
                ent->client->layout = LAYOUT_SCORES;
            }
        }
    }
}

/**
 * Hide scores layout from players
 */
void G_HideScores(arena_t *a) {
    uint8_t i, j;
    edict_t *ent;
    if (!a) {
        gi.dprintf("%s(): null arena\n", __func__);
        return;
    }
    // for each team
    for (i = 0; i < a->team_count; i++) {
        // for each player
        for (j = 0; j < MAX_TEAM_PLAYERS; j++) {
            ent = a->teams[i].players[j];

            if (ent && ent->inuse) {
                ent->client->layout = 0;
            }
        }
    }
}

/**
 * Arenas rounds can be set with time limits, where the round is over when the
 * clock reaches 00:00 regardless if more than 1 team is alive at the time.
 * The default is to just let the action play out until there is one team left,
 * in which case the clock will just count up keeping track of how long the
 * round took.
 */
void G_StartRoundClock(arena_t *a) {
    arena_clock_t *c;
    if (!a) {
        gi.dprintf("%s(): null arena\n", __func__);
        return;
    }
    c = &a->clock;
    if (a->timelimit > 0) {
        ClockInit(c, a, "round", a->timelimit * 60, 0, CLOCK_DOWN);
        c->finish = (void *) G_RoundTimelimitHit;
    } else {
        ClockInit(c, a, "round", 0, 0, CLOCK_UP);
    }
    c->postthink = (void *) ClockPostThink;
    ClockStart(c);
}

/**
 * Round timelimit has been hit, regardless if more than one team is still
 * alive, start intermission.
 */
void G_RoundTimelimitHit(arena_clock_t *c, arena_t *a) {
    G_BeginRoundIntermission(a);
}

/**
 * Begin a round
 */
void G_StartRound(arena_t *a) {
    uint8_t i;
    if (!a) {
        gi.dprintf("%s(): null arena\n", __func__);
        return;
    }
    for (i=0; i<a->team_count; i++) {
        a->teams[i].players_alive = a->teams[i].player_count;
    }

    a->state = ARENA_STATE_PLAY;
    a->round_start_frame = a->round_frame - SECS_TO_FRAMES(1);

    G_Centerprintf(a, "Fight!");
    G_ArenaSound(a, level.sounds.secret);
    G_StartRoundClock(a);
}

/**
 * Switches the player's gun-in-hand after spawning
 */
void G_StartingWeapon(edict_t *ent) {
    if (!ent->client) {
        return;
    }

    ent->client->newweapon = FindItem("rocket launcher");
    ChangeWeapon(ent);
}

/**
 * Get time string (mm:ss) from seconds
 */
void G_SecsToString(char *out, int seconds) {
    int mins;

    mins = seconds / 60;
    seconds -= (mins * 60);

    sprintf(out, "%.2d:%.2d", mins, seconds);
}

/**
 * Get a pointer to the team that won a match (if there is a clear winner)
 */
arena_team_t *G_GetWinningTeam(arena_t *a) {
    uint8_t i;

    static uint8_t alivecount = 0;
    static arena_team_t *winner;

    if (!a) {
        return NULL;
    }

    for (i=0; i<a->team_count; i++) {
        if (a->teams[i].players_alive > 0) {
            alivecount++;
            winner = &a->teams[i];
        }
    }
    
    if (alivecount > 1) {
        winner = NULL;
    }

    alivecount = 0;
    return winner;
}

/**
 * Arena settings can be overridden by files in the mapcfg folder. Those settings (if present)
 * get applied to the level structure here.
 *
 */
void G_MergeArenaSettings(arena_t *a, arena_entry_t *m) {
    if (!a) {
        return;
    }

    // for maps not in the list, there will be no arenas listed
    // just inject cvar defaults
    if (!m) {
        a->damage_flags = (int) g_damage_flags->value;
        a->weapon_flags = (int) g_weapon_flags->value;
        a->round_limit = (int) g_round_limit->value;
        a->health = (int) g_health_start->value;
        a->armor = (int) g_armor_start->value;
        a->ammo[ITEM_SLUGS] = (int) g_ammo_slugs->value;
        a->ammo[ITEM_ROCKETS] = (int) g_ammo_rockets->value;
        a->ammo[ITEM_CELLS] = (int) g_ammo_cells->value;
        a->ammo[ITEM_GRENADES] = (int) g_ammo_grenades->value;
        a->ammo[ITEM_BULLETS] = (int) g_ammo_bullets->value;
        a->ammo[ITEM_SHELLS] = (int) g_ammo_shells->value;
        a->team_count = (int) g_team_count->value;
        a->timelimit = (int) g_round_timelimit->value;
        a->fastswitch = (int) g_fast_weapon_change->value;
        a->mode = ARENA_MODE_NORMAL;
        a->corpseview = qfalse;
        memset(&a->infinite, 0, sizeof(a->infinite));
        return;
    }

    if (m->teams) {
        a->team_count = m->teams;
    } else {
        a->team_count = (int) g_team_count->value;
    }

    if (m->damage_flags) {
        a->damage_flags = m->damage_flags;
    } else {
        a->damage_flags = (int) g_damage_flags->value;
    }

    if (m->weapon_flags) {
        a->weapon_flags = m->weapon_flags;
    } else {
        a->weapon_flags = (int) g_weapon_flags->value;
    }
    
    if (m->rounds) {
        a->round_limit = m->rounds;
    } else {
        a->round_limit = (int) g_round_limit->value;
    }

    if (m->health) {
        a->health = m->health;
    } else {
        a->health = (int) g_health_start->value;
    }

    if (m->armor) {
        a->armor = m->armor;
    } else {
        a->armor = (int) g_armor_start->value;
    }
    
    if (m->ammo[ITEM_SLUGS]) {
        a->ammo[ITEM_SLUGS] = m->ammo[ITEM_SLUGS];
    } else {
        a->ammo[ITEM_SLUGS] = (int) g_ammo_slugs->value;
    }
    
    if (m->ammo[ITEM_ROCKETS]) {
        a->ammo[ITEM_ROCKETS] = m->ammo[ITEM_ROCKETS];
    } else {
        a->ammo[ITEM_ROCKETS] = (int) g_ammo_rockets->value;
    }
    
    if (m->ammo[ITEM_CELLS]) {
        a->ammo[ITEM_CELLS] = m->ammo[ITEM_CELLS];
    } else {
        a->ammo[ITEM_CELLS] = (int) g_ammo_cells->value;
    }
    
    if (m->ammo[ITEM_GRENADES]) {
        a->ammo[ITEM_GRENADES] = m->ammo[ITEM_GRENADES];
    } else {
        a->ammo[ITEM_GRENADES] = (int) g_ammo_grenades->value;
    }
    
    if (m->ammo[ITEM_BULLETS]) {
        a->ammo[ITEM_BULLETS] = m->ammo[ITEM_BULLETS];
    } else {
        a->ammo[ITEM_BULLETS] = (int) g_ammo_bullets->value;
    }
    
    if (m->ammo[ITEM_SHELLS]) {
        a->ammo[ITEM_SHELLS] = m->ammo[ITEM_SHELLS];
    } else {
        a->ammo[ITEM_SHELLS] = (int) g_ammo_shells->value;
    }

    if (m->timelimit) {
        a->timelimit = m->timelimit;
    }

    a->fastswitch = m->fastswitch;
    if (m->mode) {
        a->mode = m->mode;
    }

    if (m->corpseview) {
        a->corpseview = m->corpseview;
    }
    memcpy(a->infinite, m->infinite, sizeof(a->infinite));

    // save defaults for voting
    memcpy(a->defaultammo, a->ammo, sizeof(a->defaultammo));
    memcpy(a->defaultinfinite, a->infinite, sizeof(a->defaultinfinite));
    a->original_damage_flags = a->damage_flags;
    a->original_weapon_flags = a->weapon_flags;

    a->modified = false;
}

/**
 * Are the two players teammates?
 */
qboolean G_Teammates(edict_t *p1, edict_t *p2) {
    if (!(p1->client && p2->client)) {
        return qfalse;
    }
    return TEAM(p1) == TEAM(p2);
}

/**
 * Are the two players in the same arena?
 */
qboolean G_Arenamates(edict_t *p1, edict_t *p2) {
    if (!(p1->client && p2->client)) {
        return qfalse;
    }
    return ARENA(p1) == ARENA(p2);
}

/**
 * Read in the per-arena map config file and fill in the arena_entry structure
 *
 * return the number of arenas found in the file.
 */
size_t G_ParseMapSettings(arena_entry_t *entry, const char *mapname) {
    size_t len;
    int count;
    char path[MAX_OSPATH];
    char buffer[MAX_STRING_CHARS];
    FILE *fp;
    int arena_num;
    const char *fp_data;
    char *token;
    qboolean inarena;
    temp_weaponflags_t twf;

    count = 0;

    len = Q_concat(path, sizeof(path), game.dir, "/mapcfg/", mapname, ".cfg", NULL);

    if (len == 0) {
        return 0;
    }

    fp = fopen(path, "r");

    if (fp) {
        arena_num = -1;

        while (qtrue) {
            fp_data = fgets(buffer, sizeof(buffer), fp);
            if (!fp_data) {
                break;
            }

            if (fp_data[0] == '#' || fp_data[0] == '/') {
                continue;
            }

            token = COM_Parse(&fp_data);
            if (!*token) {
                continue;
            }

            if (Q_strcasecmp(token, "{") == 0) {
                inarena = true;
            }

            if (Q_strcasecmp(token, "}") == 0) {
                inarena = false;
            }

            if (Q_strcasecmp(token, "arena") == 0 && inarena) {
                arena_num = atoi(COM_Parse(&fp_data));
                if (arena_num >= MAX_ARENAS) {
                    continue;
                }

                count++;

                // tough seeing "0" as a value set vs not included
                entry[arena_num].fastswitch = (int) g_fast_weapon_change->value;
            }

            if (Q_strcasecmp(token, "teams") == 0 && inarena) {
                entry[arena_num].teams = atoi(COM_Parse(&fp_data));
            }

            if (Q_strcasecmp(token, "damage") == 0 && inarena) {
                G_ParseDamageString(NULL, NULL, &fp_data, &entry[arena_num].damage_flags);
            }

            if (Q_strcasecmp(token, "weapons") == 0 && inarena) {
                memset(&twf, 0, sizeof(temp_weaponflags_t));
                G_ParseWeaponString(NULL, NULL, &fp_data, &twf);
                entry[arena_num].weapon_flags = twf.weaponflags;
                memcpy(entry[arena_num].ammo, twf.ammo, sizeof(entry[arena_num].ammo));
                memcpy(entry[arena_num].infinite, twf.infinite, sizeof(entry[arena_num].infinite));
            }

            if (Q_strcasecmp(token, "rounds") == 0 && inarena) {
                entry[arena_num].rounds = atoi(COM_Parse(&fp_data));
            }

            if (Q_strcasecmp(token, "health") == 0 && inarena) {
                entry[arena_num].health = atoi(COM_Parse(&fp_data));
            }

            if (Q_strcasecmp(token, "armor") == 0 && inarena) {
                entry[arena_num].armor = atoi(COM_Parse(&fp_data));
            }
            
            if (Q_strcasecmp(token, "slugs") == 0 && inarena) {
                entry[arena_num].ammo[ITEM_SLUGS] = atoi(COM_Parse(&fp_data));
            }
            
            if (Q_strcasecmp(token, "rockets") == 0 && inarena) {
                entry[arena_num].ammo[ITEM_ROCKETS] = atoi(COM_Parse(&fp_data));
            }
            
            if (Q_strcasecmp(token, "cells") == 0 && inarena) {
                entry[arena_num].ammo[ITEM_CELLS] = atoi(COM_Parse(&fp_data));
            }
            
            if (Q_strcasecmp(token, "grenades") == 0 && inarena) {
                entry[arena_num].ammo[ITEM_GRENADES] = atoi(COM_Parse(&fp_data));
            }
            
            if (Q_strcasecmp(token, "bullets") == 0 && inarena) {
                entry[arena_num].ammo[ITEM_BULLETS] = atoi(COM_Parse(&fp_data));
            }
            
            if (Q_strcasecmp(token, "shells") == 0 && inarena) {
                entry[arena_num].ammo[ITEM_SHELLS] = atoi(COM_Parse(&fp_data));
            }

            if (Q_strcasecmp(token, "timelimit") == 0 && inarena) {
                entry[arena_num].timelimit = atoi(COM_Parse(&fp_data));
            }

            if (Q_strcasecmp(token, "fastswitch") == 0 && inarena) {
                entry[arena_num].fastswitch = atoi(COM_Parse(&fp_data));
            }

            if (Q_strcasecmp(token, "mode") == 0 && inarena) {
                entry[arena_num].mode = atoi(COM_Parse(&fp_data));
            }

            if (Q_strcasecmp(token, "corpseview") == 0 && inarena) {
                entry[arena_num].corpseview = atoi(COM_Parse(&fp_data));
            }
        }
        fclose(fp);
    }
    return count;
}

/**
 * Set the best weapon available as current
 */
void G_SelectBestWeapon(edict_t *ent) {
    if (!ent) {
        return;
    }

    if (!ent->client) {
        return;
    }

    if (ent->client->inventory[ITEM_RAILGUN] > 0) {
        ent->client->newweapon = FindItem("railgun");

    } else if (ent->client->inventory[ITEM_ROCKETLAUNCHER] > 0) {
        ent->client->newweapon = FindItem("rocket launcher");

    } else if (ent->client->inventory[ITEM_HYPERBLASTER] > 0) {
        ent->client->newweapon = FindItem("hyperblaster");

    } else if (ent->client->inventory[ITEM_CHAINGUN] > 0) {
        ent->client->newweapon = FindItem("chaingun");

    } else if (ent->client->inventory[ITEM_SUPERSHOTGUN] > 0) {
        ent->client->newweapon = FindItem("super shotgun");

    } else if (ent->client->inventory[ITEM_GRENADELAUNCHER] > 0) {
        ent->client->newweapon = FindItem("grenade launcher");

    } else if (ent->client->inventory[ITEM_MACHINEGUN] > 0) {
        ent->client->newweapon = FindItem("machinegun");

    } else if (ent->client->inventory[ITEM_SHOTGUN] > 0) {
        ent->client->newweapon = FindItem("shotgun");

    } else {
        ent->client->newweapon = FindItem("blaster");
    }

    ChangeWeapon(ent);
}

/**
 * Get the team ready to play again
 */
void G_ResetTeam(arena_team_t *t) {
    uint8_t i;
    edict_t *player;

    t->damage_dealt = 0;
    t->damage_taken = 0;
    t->ready = qfalse;

    // respawn all players
    for (i = 0; i < MAX_TEAM_PLAYERS; i++) {
        if (t->players[i]) {
            player = t->players[i];

            memset(&player->client->resp, 0, sizeof(player->client->resp));
            memset(&player->client->level.vote, 0, sizeof(player->client->level.vote));

            player->movetype = MOVETYPE_NOCLIP; // don't leave a body
            player->client->pers.ready = qfalse;

            if (g_team_reset->value) {
                G_TeamPart(player, true);
            } else {
                G_RespawnPlayer(player);
            }
        }
    }
}

/**
 * Re-initialize an arena when settings are changed
 */
void G_RecreateArena(arena_t *a) {
    uint8_t i;

    // remove all players from all teams
    for (i=0; i<a->team_count; i++) {
        G_RemoveAllTeamPlayers(&a->teams[i], qtrue);
    }
    G_InitArenaTeams(a);
}

/**
 * Drop every player from a team
 */
void G_RemoveAllTeamPlayers(arena_team_t *team, qboolean silent) {
    uint8_t i;
    for (i=0; i<MAX_TEAM_PLAYERS; i++) {
        if (!team->players[i]) {
            continue;
        }
        G_TeamPart(team->players[i], silent);
    }
}

/**
 * Return an arena to it's original state. Called after match ends
 */
void G_ResetArena(arena_t *a) {
    uint8_t i;

    a->intermission_framenum = 0;
    a->intermission_exit = 0;
    a->state = ARENA_STATE_WARMUP;
    a->ready = qfalse;
    a->teams_alive = a->team_count;
    a->current_round = 1;

    G_ConfigString(a, CS_ROUND, G_RoundToString(a));
    G_UpdateConfigStrings(a);

    for (i=0; i<a->team_count; i++) {
        G_ResetTeam(&a->teams[i]);
    }

    G_FinishArenaVote(a);
}

/**
 * Similar to gi.multicast() but only sends the msg buffer to the members of 1 arena
 */
void G_ArenaCast(arena_t *a, qboolean reliable) {
    uint8_t i;
    arena_team_t *team;

    for (i=0; i<a->team_count; i++) {
        team = &a->teams[i];
        G_TeamCast(team, reliable);
    }
}


/**
 * Just like gi.multicast() but to every team player
 */
void G_TeamCast(arena_team_t *t, qboolean reliable) {
    uint8_t i;
    edict_t *ent;

    for (i=0; i<MAX_TEAM_PLAYERS; i++) {
        ent = t->players[i];
        if (ent && ent->inuse) {
            gi.unicast(ent, reliable);
        }
    }
}

/**
 * Get the english version of the weapon flags
 */
char *G_WeaponFlagsToString(arena_t *a) {
    static char str[200];

    memset(&str, 0, sizeof(str));

    char inf[4];
    G_AsciiToConsole(inf, "inf");

    if (a->weapon_flags & ARENAWEAPON_SHOTGUN) {
        if (a->infinite[ITEM_SHELLS]) {
            strcat(str, va("sg:%s, ", inf));
        } else {
            strcat(str, va("sg:%d, ", a->ammo[ITEM_SHELLS]));
        }
    }

    if (a->weapon_flags & ARENAWEAPON_SUPERSHOTGUN) {
        if (a->infinite[ITEM_SHELLS]) {
            strcat(str, va("ssg:%s, ", inf));
        } else {
            strcat(str, va("ssg:%d, ", a->ammo[ITEM_SHELLS]));
        }
    }

    if (a->weapon_flags & ARENAWEAPON_MACHINEGUN) {
        if (a->infinite[ITEM_BULLETS]) {
            strcat(str, va("mg:%s, ", inf));
        } else {
            strcat(str, va("mg:%d, ", a->ammo[ITEM_BULLETS]));
        }
    }

    if (a->weapon_flags & ARENAWEAPON_CHAINGUN) {
        if (a->infinite[ITEM_BULLETS]) {
            strcat(str, va("cg:%s, ", inf));
        } else {
            strcat(str, va("cg:%d, ", a->ammo[ITEM_BULLETS]));
        }
    }

    if (a->weapon_flags & ARENAWEAPON_GRENADE) {
        if (a->infinite[ITEM_GRENADES]) {
            strcat(str, va("gr:%s, ", inf));
        } else {
            strcat(str, va("gr:%d, ", a->ammo[ITEM_GRENADES]));
        }
    }

    if (a->weapon_flags & ARENAWEAPON_GRENADELAUNCHER) {
        if (a->infinite[ITEM_GRENADES]) {
            strcat(str, va("gl:%s, ", inf));
        } else {
            strcat(str, va("gl:%d, ", a->ammo[ITEM_GRENADES]));
        }
    }

    if (a->weapon_flags & ARENAWEAPON_HYPERBLASTER) {
        if (a->infinite[ITEM_CELLS]) {
            strcat(str, va("hb:%s, ", inf));
        } else {
            strcat(str, va("hb:%d, ", a->ammo[ITEM_CELLS]));
        }
    }

    if (a->weapon_flags & ARENAWEAPON_ROCKETLAUNCHER) {
        if (a->infinite[ITEM_ROCKETS]) {
            strcat(str, va("rl:%s, ", inf));
        } else {
            strcat(str, va("rl:%d, ", a->ammo[ITEM_ROCKETS]));
        }
    }

    if (a->weapon_flags & ARENAWEAPON_RAILGUN) {
        if (a->infinite[ITEM_SLUGS]) {
            strcat(str, va("rg:%s, ", inf));
        } else {
            strcat(str, va("rg:%d, ", a->ammo[ITEM_SLUGS]));
        }
    }

    if (a->weapon_flags & ARENAWEAPON_BFG) {
        if (a->infinite[ITEM_CELLS]) {
            strcat(str, va("bfg:%s, ", inf));
        } else {
            strcat(str, va("bfg:%d, ", a->ammo[ITEM_CELLS]));
        }
    }

    str[strlen(str) -2] = 0;
    return str;
}

/**
 * Get the english version of damage flags
 */
char *G_DamageFlagsToString(uint32_t df) {
    static char str[200];

    memset(&str, 0, sizeof(str));

    if (df == 0) {
        return "none";
    }

    if (df & ARENADAMAGE_SELF) {
        strcat(str, "self health, ");
    }

    if (df & ARENADAMAGE_SELF_ARMOR) {
        strcat(str, "self armor, ");
    }

    if (df & ARENADAMAGE_TEAM) {
        strcat(str, "team health, ");
    }

    if (df & ARENADAMAGE_TEAM_ARMOR) {
        strcat(str, "team armor, ");
    }

    if (df & ARENADAMAGE_FALL) {
        strcat(str, "falling, ");
    }

    if (strlen(str) > 1) {
        str[strlen(str) - 2] = 0;
    }

    return str;
}

/**
 * Random ammo quantities could be fun.
 *
 * Maybe call this at match start rather than before the countdown for an
 * element of surprise.
 */
void G_RandomizeAmmo(uint16_t *out) {
    out[ITEM_SHELLS]   = genrand_int32() & 0xF;
    out[ITEM_BULLETS]  = genrand_int32() & 0xFF;
    out[ITEM_GRENADES] = genrand_int32() & 0xF;
    out[ITEM_CELLS]    = genrand_int32() & 0xFF;
    out[ITEM_ROCKETS]  = genrand_int32() & 0xF;
    out[ITEM_SLUGS]    = genrand_int32() & 0xF;
}

/**
 * Generate a player-specific statusbar
 */
const char *G_CreatePlayerStatusBar(edict_t *player) {
    static char *statusbar;
    static char weaponhud[175];        // the weapon icons
    static char ammohud[135];        // the ammo counts
    int         hud_x,
                hud_y;

    if (!player->client) {
        return "";
    }

    hud_y = 0;
    hud_x = -25;

    weaponhud[0] = 0;
    ammohud[0]   = 0;

    // set x position at first for all weapon icons, to save the chars since CS max is 1000
    strcpy(weaponhud, va("xr %d ", hud_x));

    // set x position for ammo quantities ^
    strcpy(ammohud, va("xr %d ", hud_x - 50));

    // super/shotgun
    if (player->client->inventory[ITEM_SUPERSHOTGUN]) {
        strcat(weaponhud, va("yv %d picn w_sshotgun ", hud_y));
        strcat(ammohud, va("yv %d num 3 %d ", hud_y, STAT_AMMO_SHELLS));
        hud_y += 25;
    } else if (player->client->inventory[ITEM_SHOTGUN]) {
        strcat(weaponhud, va("yv %d picn w_shotgun ", hud_y));
        strcat(ammohud, va("yv %d num 3 %d ", hud_y, STAT_AMMO_SHELLS));
        hud_y += 25;
    }

    // chaingun/machinegun
    if (player->client->inventory[ITEM_CHAINGUN]) {
        strcat(weaponhud, va("yv %d picn w_chaingun ", hud_y));
        strcat(ammohud, va("yv %d num 3 %d ", hud_y, STAT_AMMO_BULLETS));
        hud_y += 25;
    } else if (player->client->inventory[ITEM_MACHINEGUN]) {
        strcat(weaponhud, va("yv %d picn w_machinegun ", hud_y));
        strcat(ammohud, va("yv %d num 3 %d ", hud_y, STAT_AMMO_SHELLS));
        hud_y += 25;
    }

    // hand grenades/launcher
    if (player->client->inventory[ITEM_GRENADELAUNCHER]) {
        strcat(weaponhud, va("yv %d picn w_glauncher ", hud_y));
        strcat(ammohud, va("yv %d num 3 %d ", hud_y, STAT_AMMO_GRENADES));
        hud_y += 25;
    } else if (player->client->inventory[ITEM_GRENADES]) {
        strcat(weaponhud, va("yv %d picn w_hgrenade ", hud_y));
        strcat(ammohud, va("yv %d num 3 %d ", hud_y, STAT_AMMO_GRENADES));
        hud_y += 25;
    }

    // hyper blaster
    if (player->client->inventory[ITEM_HYPERBLASTER]) {
        strcat(weaponhud, va("yv %d picn w_hyperblaster ", hud_y));
        strcat(ammohud, va("yv %d num 3 %d ", hud_y, STAT_AMMO_CELLS));
        hud_y += 25;
    }

    // rocket launcher
    if (player->client->inventory[ITEM_ROCKETLAUNCHER]) {
        strcat(weaponhud, va("yv %d picn w_rlauncher ", hud_y));
        strcat(ammohud, va("yv %d num 3 %d ", hud_y, STAT_AMMO_ROCKETS));
        hud_y += 25;
    }

    // railgun
    if (player->client->inventory[ITEM_RAILGUN]) {
        strcat(weaponhud, va("yv %d picn w_railgun ", hud_y));
        strcat(ammohud, va("yv %d num 3 %d ", hud_y, STAT_AMMO_SLUGS));
        hud_y += 25;
    }

    // BFG
    if (player->client->inventory[ITEM_BFG]) {
        strcat(weaponhud, va("yv %d picn w_bfg ", hud_y));
        strcat(ammohud, va("yv %d num 3 %d ", hud_y, STAT_AMMO_CELLS));
        hud_y += 25;
    }

    statusbar = va(
        "yb -24 "

        // health
        "xv 0 "
        "hnum "
        "xv 50 "
        "pic 0 "

        // ammo
        "if 2 "
            "xv 100 "
            "anum "
            "xv 150 "
            "pic 7 " // STAT_WEAPON_ICON
        "endif "

        // armor
        "if 4 "
            "xv 200 "
            "rnum "
            "xv 250 "
            "pic 4 "
        "endif "

        "yb -50 "

        // timer 1 (quad, enviro, breather)
        "if 9 "
            "xv 246 "
            "num 2 10 "
            "xv 296 "
            "pic 9 "
        "endif "

        //  help / weapon icon
        "if 11 "
            "xv 148 "
            "pic 11 "
        "endif "

        // points (damage dealt)
        "xr -80 "
        "yt 2 "
        "num 5 14 "


        // countdown
        "if 29 "
            "yv 100 "
            "xv 150 "
            "num 2 29 "
        "endif "

        // chase camera
        "if 16 "
            "xv 0 "
            "yb -68 "
            "string Chasing "
            "xv 64 "
            "stat_string 16 "
        "endif "

        // spectator
        "if 17 "
            "xv 0 "
            "yb -58 "
            "stat_string 17 "
        "endif "

        // view id
        "if 24 "
            "xv 150 "
            "yb -35 "
            "stat_string 24 "
        "endif "

        // vote proposal
        "if 25 "
            "xl 10 "
            "yb -188 "
            "stat_string 25 "
            "yb -180 "
            "stat_string 26 "
        "endif "

        // match status
        "if 31 "
            "yb -35 "
            "xl 8 "
            "stat_string 31 "
        "endif "

        // round
        "if 28 "
            "yb -35 "
            "xr -96 "
            "stat_string 28 "
        "endif "
        "%s"
        "%s",
        weaponhud, ammohud
    );

    return statusbar;
}

/**
 * Generate a spec-specific statusbar
 */
const char *G_CreateSpectatorStatusBar(edict_t *player) {
    static char *statusbar;

    if (!player->client) {
        return "";
    }

    statusbar = va(
        "yb -24 "

        // health
        //"xv 0 "
        //"hnum "
        //"xv 50 "
        //"pic 0 "

        // ammo
        //"if 2 "
        //    "xv 100 "
        //    "anum "
        //    "xv 150 "
        //    "pic 2 "
        //"endif "

        // armor
        //"if 4 "
        //    "xv 200 "
        //    "rnum "
        //    "xv 250 "
        //    "pic 4 "
        //"endif "

        "yb -50 "

        // picked up item
        //"if 7 "
        //    "xv 0 "
        //    "pic 7 "
        //    "xv 26 "
        //    "yb -42 "
        //    "stat_string 8 "
        //    "yb -50 "
        //"endif "

        // timer 1 (quad, enviro, breather)
        "if 9 "
            "xv 246 "
            "num 2 10 "
            "xv 296 "
            "pic 9 "
        "endif "

        //  help / weapon icon
        //"if 11 "
        //    "xv 148 "
        //    "pic 11 "
        //"endif "

        // points (damage dealt)
        "xr -80 "
        "yt 2 "
        "num 5 14 "


        // countdown
        "if 29 "
            "yv 100 "
            "xv 150 "
            "num 2 29 "
        "endif "

        // chase camera
        "if 16 "
            "xv 0 "
            "yb -68 "
            "string Chasing "
            "xv 64 "
            "stat_string 16 "
        "endif "

        // spectator
        "if 17 "
            "xv 0 "
            "yb -58 "
            "stat_string 17 "
        "endif "

        // view id
        "if 24 "
            "xv -100 "
            "yb -80 "
            "string Viewing "
            "xv -36 "
            "stat_string 24 "
        "endif "

        // vote proposal
        "if 25 "
            "xl 10 "
            "yb -188 "
            "stat_string 25 "
            "yb -180 "
            "stat_string 26 "
        "endif "

        // match status
        "if 31 "
            "yb -35 "
            "xl 8 "
            "stat_string 31 "
        "endif "

        // round
        "if 28 "
            "yb -35 "
            "xr -96 "
            "stat_string 28 "
        "endif "
    );

    return statusbar;
}

/**
 * Send player/spec their specific statusbar
 */
void G_SendStatusBar(edict_t *ent) {
    gi.WriteByte(SVC_CONFIGSTRING);
    gi.WriteShort(CS_STATUSBAR);

    if (TEAM(ent) || ent->client->chase_target) {
        gi.WriteString(G_CreatePlayerStatusBar(ent));
        ent->client->pers.current_statusbar = SB_PLAYER;
    } else {
        gi.WriteString(G_CreateSpectatorStatusBar(ent));
        ent->client->pers.current_statusbar = SB_SPEC;
    }

    gi.unicast(ent, true);
}

/**
 * Update statusbars for all arena players
 */
void G_UpdatePlayerStatusBars(arena_t *a) {
    uint8_t i;

    for (i=0; i<game.maxclients; i++) {
        if (!a->clients[i]) {
            continue;
        }
        G_SendStatusBar(a->clients[i]);
    }
}

/**
 * A short break between rounds (scoreboard is shown)
 *
 * Triggered when all but 1 team are dead or timelimit expires
 */
void G_BeginRoundIntermission(arena_t *a) {
    if (a->state == ARENA_STATE_RINTERMISSION) {
        return;
    }
    a->state = ARENA_STATE_RINTERMISSION;
    G_ShowScores(a);
    ClockStartIntermission(a);
}

/**
 * Round intermission is over, reset and proceed to next round
 */
void G_EndRoundIntermission(arena_t *a) {
    if (a->state != ARENA_STATE_RINTERMISSION) {
        return;
    }
    G_EndRound(a, NULL);
}

/**
 * Reset this arena back to the defaults
 */
void G_ApplyDefaults(arena_t *a) {
    uint8_t i;

    if (!a) {
        return;
    }

    for (i=0; i<a->team_count; i++) {
        G_RemoveAllTeamPlayers(&a->teams[i], qtrue);
    }

    G_MergeArenaSettings(a, &level.arena_defaults[a->number]);
    G_InitArenaTeams(a);
}

/**
 * Get a string value for the arena mode
 */
char *G_ArenaModeString(arena_t *a) {
    switch (a->mode) {
    case ARENA_MODE_COMPETITION:
        return va("competition");
        break;
    case ARENA_MODE_REDROVER:
        return va("red rover");
        break;
    default:
        return va("normal");
    }
}

/**
 * Sets the teamskin property of an player and
 * applies it to all team members. This does not
 * set the global skin for a particular team, but
 * the specific skin one player wants to use for
 * their team. This allows players to override
 * the team default skin for whatever reason.
 *
 * Called when a player manually types "tskin"
 * command.
 */
void G_SetTSkin(edict_t *target) {
    edict_t *ent;

    if (!target->client->pers.teamskin[0]) {
        return;
    }
    for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++) {
        if (!ent->inuse) {
            continue;
        }
        if (!G_Teammates(ent, target)) {
            continue;
        }
        gi.WriteByte(SVC_CONFIGSTRING);
        gi.WriteShort(CS_PLAYERSKINS + (ent - g_edicts) -1);
        gi.WriteString(va("%s\\%s", NAME(ent), target->client->pers.teamskin));
        gi.unicast(target, true);
    }
}

/**
 * Sets the enemyskin property of an player and
 * applies it to all enemy players.
 *
 * Called when a player manually types "eskin"
 * command.
 */
void G_SetESkin(edict_t *target) {
    edict_t *ent;

    if (!target->client->pers.enemyskin[0]) {
        return;
    }
    for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++) {
        if (!ent->inuse) {
            continue;
        }
        if (!IS_PLAYER(ent)) {
            continue;
        }
        if (G_Teammates(ent, target)) {
            continue;
        }
        gi.WriteByte(SVC_CONFIGSTRING);
        gi.WriteShort(CS_PLAYERSKINS + (ent - g_edicts) -1);
        gi.WriteString(va("%s\\%s", NAME(ent), target->client->pers.enemyskin));
        gi.unicast(target, true);
    }
}

/**
 * Figure which skin should be seen for a player
 * and who should see it, then send configstrings.
 *
 * Called when a player joins a team. Someone people
 * will have tskin and eskin set, so they'll get
 * custom skins, and everyone else will get the skin
 * associated with that particular team.
 *
 */
void G_SetSkin(edict_t *skinfor) {
    edict_t *ent;
    char *string;

    if (!skinfor->client) {
        return;
    }
    for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++) {
        if (!ent->inuse) {
            continue;
        }
        if (!G_Arenamates(ent, skinfor)) {
            continue;
        }
        // the default skin associated with skinfor's team
        string = va("%s\\%s", NAME(skinfor), TEAM(skinfor)->skin);

        if (G_Teammates(ent, skinfor) && ent->client->pers.teamskin[0]) {
            string = va("%s\\%s", NAME(skinfor), ent->client->pers.teamskin);
        }

        if (!G_Teammates(ent, skinfor) && ent->client->pers.enemyskin[0]) {
            string = va("%s\\%s", NAME(skinfor), ent->client->pers.enemyskin);
        }

        gi.WriteByte(SVC_CONFIGSTRING);
        gi.WriteShort(CS_PLAYERSKINS + (skinfor - g_edicts) - 1);
        gi.WriteString(string);
        gi.unicast(ent, qtrue);
    }
}

/**
 * Check each player on the team for their ready status. If nobody
 * is not ready, set the overall team status to ready.
 */
void G_CheckTeamReady(arena_team_t *t) {
    uint8_t i;

    if (t->player_count == 0) {
        return;
    }
    for (i=0; i<MAX_TEAM_PLAYERS; i++) {
        if (!t->players[i]) {
            continue;
        }
        if (!t->players[i]->client->pers.ready) {
            gi.dprintf("%s is not ready\n", NAME(t->players[i]));
            t->ready = qfalse;
            return;
        }
    }
    t->ready = qtrue;
}

/**
 * Check each team in the arena for their ready status. If no team
 * is not ready, then set the overall arena status to ready. This
 * will trigger the start of a match.
 */
void G_CheckArenaReady(arena_t *a) {
    uint8_t i;
    arena_team_t *t;

    for (i=0; i<a->team_count; i++) {
        t = &a->teams[i];
        if (!t) {
            continue;
        }
        if (!t->ready) {
            gi.dprintf("%s is not ready\n", t->name);
            a->ready = qfalse;
            return;
        }
    }
    a->ready = qtrue;
}

/**
 * Check if anyone on the team is still alive.
 *
 * Called when a player dies or the team population changes (joins/quits/etc).
 */
qboolean G_CheckTeamAlive(edict_t *ent) {
    uint8_t i;
    arena_team_t *t = TEAM(ent);

    for (i=0; i<MAX_TEAM_PLAYERS; i++) {
        if (!t->players[i]) {
            continue;
        }
        if (t->players[i]->health > 0) {
            return qtrue;
        }
    }
    return qfalse;
}

/**
 * Check if a round is being played, if that round
 * is over (only 1 team left alive). Used to determine
 * if arena should proceed into intermission
 */
qboolean G_IsRoundOver(arena_t *a) {
    if (!a) {
        return qfalse;
    }
    if (a->teams_alive <= 1) {
        return qtrue;
    }
    return qfalse;
}

/**
 * Move the player to the place in the map where they will spawn
 */
void G_MovePlayerToSpawnSpot(edict_t *ent, edict_t *spot) {
    VectorCopy(spot->s.origin, ent->s.origin);
    VectorCopy(spot->s.angles, ent->s.angles);
}

/**
 * Count everyone on all the teams in a particular arena keeping track of
 * how many players are alive.
 *
 * Called from player_die() when ever a player is fragged
 */
void G_CountEveryone(arena_t *a) {
    int8_t players_alive;
    int32_t teams_alive = 0;

    for (int i=0; i<a->team_count; i++) {
        players_alive = 0;
        for (int j=0; j<a->teams[i].player_count; j++) {
            if (!a->teams[i].players[j]) {
                continue;
            }
            if (a->teams[i].players[j]->health > 0) {
                players_alive++;
            }
        }
        a->teams[i].players_alive = players_alive;
        if (players_alive > 0) {
            teams_alive++;
        }
    }
    a->teams_alive = teams_alive;
}

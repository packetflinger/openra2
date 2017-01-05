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


arena_t *FindArena(edict_t *ent) {

	if (!ent->client)  
		return NULL;
	
	return ent->client->pers.arena;
}

static arena_team_t *FindTeam(edict_t *ent, arena_team_type_t type) {
	
	arena_t *a = FindArena(ent);
	
	if (!a) {
		return NULL;
	}
	
	if (a->team_home.type == type)
		return &(a->team_home);
	
	if (a->team_away.type == type)
		return &(a->team_away);
	
	return NULL;
}

// periodically count players to make sure none got lost
static void update_playercounts(arena_t *a) {
	
	int i;
	int count = 0;
	gclient_t *cl;
	
	for (i=0; i<game.maxclients; i++) {
		cl = &game.clients[i];
		if (cl && cl->pers.arena == a) {
			count++;
		}
	}

	a->player_count = count;
}

void change_arena(edict_t *self) {
	
	PutClientInServer(self);
	
	
	// add a teleportation effect
    self->s.event = EV_PLAYER_TELEPORT;

    // hold in place briefly
    self->client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
    self->client->ps.pmove.pm_time = 14;

    self->client->respawn_framenum = level.framenum;
	
	
}

void G_ArenaScoreboardMessage(edict_t *ent, qboolean reliable) {
    char buffer[MAX_STRING_CHARS];

	G_BuildScoreboard(buffer, ent->client, ent->client->pers.arena);

    gi.WriteByte(svc_layout);
    gi.WriteString(buffer);
    gi.unicast(ent, reliable);
}

// start a sound for all arena members, people in other arenas shouldn't hear it
void G_ArenaSound(arena_t *a, int index) {
	
	if (!a)
		return;
	
	int i;
	gclient_t *cl;
	
	for (i=0; i<game.maxclients; i++) {
		
		cl = &game.clients[i];
		if (!cl)
			continue;
		
		if (cl->pers.arena == a) {
			gi.sound(cl->edict, CHAN_AUTO, index, 1, ATTN_NORM, 0);
		}
	}
}


void G_ArenaStuff(arena_t *a, const char *command) {
	
	if (!a)
		return;
	
	int i;
	gclient_t *cl;
	
	for (i=0; i<game.maxclients; i++) {
		
		cl = &game.clients[i];
		if (!cl)
			continue;
		
		if (cl->pers.arena == a) {
			gi.WriteByte(svc_stufftext);
			gi.WriteString(command);
			gi.unicast(cl->edict, qtrue);
		}
	}
}

// check for things like state changes, start/end of rounds, timeouts, countdown clocks, etc
void G_ArenaThink(arena_t *a) {
	static qboolean foundwinner = false;
	
	if (!a)
		return;
	
	// don't waste cpu if nobody is in this arena
	if (a->player_count == 0)
		return;

	if (a->state == ARENA_STATE_TIMEOUT) {
		G_TimeoutFrame(a);
		return;
	}
	
	// end of round
	if (a->state == ARENA_STATE_PLAY) {

		arena_team_t *winner = G_GetWinningTeam(a);
		
		if (winner && !foundwinner) {
			a->round_end_frame = level.framenum + SECS_TO_FRAMES((int)g_round_end_time->value);
			foundwinner = true;
			G_ShowScores(a);
		}
		
		if (a->round_end_frame == level.framenum) {
			foundwinner = false;
			G_EndRound(a, winner);
			return;
		}
	}
	
	// start a round
	if (a->state < ARENA_STATE_PLAY && a->round_start_frame) {
		int framesleft = a->round_start_frame - level.framenum;
		
		if (framesleft > 0 && framesleft % SECS_TO_FRAMES(1) == 0) {
			
			gi.configstring(CS_ARENA_ROUNDS + a->number, G_RoundToString(a));
			a->countdown = FRAMES_TO_SECS(framesleft);
		} else if (framesleft == 0) {
			
			G_StartRound(a);
			return;
		}
	}
	
	// pregame
	if (a->state == ARENA_STATE_WARMUP) {
		if (G_CheckReady(a)) {	// is everyone ready?
			a->state = ARENA_STATE_COUNTDOWN;
			a->current_round = 1;
			a->round_start_frame = level.framenum + SECS_TO_FRAMES((int)g_round_countdown->value);
			a->countdown = (int)g_round_countdown->value;
			
			G_RespawnPlayers(a);
		}
	}
	
	G_CheckTimers(a);
}

// broadcast print to only members of specified arena
void G_bprintf(arena_t *arena, int level, const char *fmt, ...) {
	va_list     argptr;
    char        string[MAX_STRING_CHARS];
    size_t      len;
    int         i;
	edict_t		*other;
	
    va_start(argptr, fmt);
    len = Q_vsnprintf(string, sizeof(string), fmt, argptr);
    va_end(argptr);

	
    if (len >= sizeof(string)) {
        return;
    }
	
	for (i = 1; i <= game.maxclients; i++) {
        other = &g_edicts[i];
		
        if (!other->inuse)
            continue;
		
        if (!other->client)
            continue;
		
        if (arena != other->client->pers.arena)
			continue;
		
        gi.cprintf(other, level, "%s", string);
    }
}

// build the arena menu structure
void G_BuildMenu(void) {
	
	int i, j;
	for (i=0, j=0; i<MAX_ARENAS; i++) {
		if (level.arenas[i].number) {
			menu_lookup[j].num = level.arenas[i].number;
			Q_strlcpy(menu_lookup[j].name, level.arenas[i].name, sizeof(menu_lookup[j].name));
			j++;
		}
	}
}

/*
==================
Used to update per-client scoreboard and build
global oldscores (client is NULL in the latter case).

Build horizontally
==================
*/
size_t G_BuildScoreboard(char *buffer, gclient_t *client, arena_t *arena)
{
    char    entry[MAX_STRING_CHARS];
    char    status[MAX_QPATH];
    char    timebuf[16];
    size_t  total, len;
    int     i, j, numranks;
    int     x, y, sec, eff;
    gclient_t   *ranks[MAX_CLIENTS];
    gclient_t   *c;
    time_t      t;
    struct tm   *tm;

	y = 20;		// starting point down from top of screen
	
	t = time(NULL);
    tm = localtime(&t);
	len = strftime(status, sizeof(status), "%b %e, %Y %H:%M ", tm);
	
	if (len < 1)
        strcpy(status, "???");
	
    if (!client) {
        Q_snprintf(entry, sizeof(entry),
                   "yt %d cstring2 \"Old scoreboard from %s\"", y, level.mapname);
    } else {
		Q_snprintf(entry, sizeof(entry), "yt %d cstring2 \"%s - %s\"", y, status, arena->name);
    }

	y += LAYOUT_LINE_HEIGHT * 6	;
	x = LAYOUT_CHAR_WIDTH * -30;
	
    total = Q_scnprintf(buffer, MAX_STRING_CHARS,
                        "xv 0 %s "
						"xv %d "
                        "yt %d "
                        "cstring \"Team %s\" "
						"xv 0 yt %d cstring \"VS.\" "
						"xv %d "
                        "yt %d "
                        "cstring2 \"Player          Frg Rnd Mch  FPH Time Ping\" ", 
						entry, x, y, arena->team_home.name, y, x, y + LAYOUT_LINE_HEIGHT * 2
	);

    numranks = G_CalcArenaRanks(ranks, &arena->team_home);

	
	
    // hometeam first, add the clients sorted by rank
    y += LAYOUT_LINE_HEIGHT * 3;
    for (i = 0; i < numranks; i++) {
        c = ranks[i];

        sec = (level.framenum - c->resp.enter_framenum) / HZ;
		
        if (!sec) {
            sec = 1;
        }

        if (c->resp.score > 0) {
            j = c->resp.score + c->resp.deaths;
            eff = j ? c->resp.score * 100 / j : 100;
        } else {
            eff = 0;
        }

        if (level.framenum < 10 * 60 * HZ) {
            sprintf(timebuf, "%d:%02d", sec / 60, sec % 60);
        } else {
            sprintf(timebuf, "%d", sec / 60);
        }

        len = Q_snprintf(entry, sizeof(entry),
                         "yt %d cstring%s \"%-15s %3d %3d %3d %4d %4s %4d\"",
                         y, c == client ? "" : "2",
                         c->pers.netname, c->resp.score, c->resp.round_score, c->resp.match_score,
                         c->resp.score * 3600 / sec, timebuf, c->ping);
						 
        if (len >= sizeof(entry))
            continue;
		
        if (total + len >= MAX_STRING_CHARS)
            break;
		
        memcpy(buffer + total, entry, len);
		
        total += len;
        y += LAYOUT_LINE_HEIGHT;
    }
	
	y = (LAYOUT_LINE_HEIGHT * 6) + 20;
	x = LAYOUT_CHAR_WIDTH * 30;
	total += Q_scnprintf(buffer + total , MAX_STRING_CHARS,
                        "xv %d "
                        "yt %d "
                        "cstring \"Team %s\""
                        "yt %d "
                        "cstring2 \"Player          Frg Rnd Mch  FPH Time Ping\"", 
						x, y, arena->team_away.name, y + LAYOUT_LINE_HEIGHT *2);
	
	numranks = G_CalcArenaRanks(ranks, &arena->team_away);

	y += LAYOUT_LINE_HEIGHT * 3;
	
    // away team second, add the clients sorted by rank
    for (i = 0; i < numranks; i++) {
        c = ranks[i];

        sec = (level.framenum - c->resp.enter_framenum) / HZ;
        if (!sec) {
            sec = 1;
        }

        if (c->resp.score > 0) {
            j = c->resp.score + c->resp.deaths;
            eff = j ? c->resp.score * 100 / j : 100;
        } else {
            eff = 0;
        }

        if (level.framenum < 10 * 60 * HZ) {
            sprintf(timebuf, "%d:%02d", sec / 60, sec % 60);
        } else {
            sprintf(timebuf, "%d", sec / 60);
        }

        len = Q_snprintf(entry, sizeof(entry),
                         "yt %d cstring%s \"%-15s %3d %3d %3d %4d %4s %4d\"",
                         y, c == client ? "" : "2",
                         c->pers.netname, c->resp.score, c->resp.round_score, c->resp.match_score,
                         c->resp.score * 3600 / sec, timebuf, c->ping);
						 
        if (len >= sizeof(entry))
            continue;

        if (total + len >= MAX_STRING_CHARS)
            break;
		
        memcpy(buffer + total, entry, len);
		
        total += len;
        y += LAYOUT_LINE_HEIGHT;
    }

	y = LAYOUT_LINE_HEIGHT * (6 + MAX_ARENA_TEAM_PLAYERS) + 20;
	
    // add spectators in fixed order
	total += Q_scnprintf(buffer + total , MAX_STRING_CHARS, "xv 0 yt %d cstring \"----- Spectators -----\"", y);
	y += LAYOUT_LINE_HEIGHT ;					
    for (i = 0, j = 0; i < game.maxclients; i++) {
        c = &game.clients[i];
		
        if (c->pers.connected != CONN_PREGAME && c->pers.connected != CONN_SPECTATOR)
            continue;
		
		if (c->pers.arena != arena)
			continue;
		
        if (c->pers.mvdspec)
            continue;

        sec = (level.framenum - c->resp.enter_framenum) / HZ;
        if (!sec) {
            sec = 1;
        }

        if (c->chase_target) {
            Q_snprintf(status, sizeof(status), "-> %.13s",
                       c->chase_target->client->pers.netname);
        } else {
            //strcpy(status, "(observing)");
			status[0] = 0;
        }

        len = Q_snprintf(entry, sizeof(entry),
                         "yt %d cstring \"%s:%d %s %d\"",
                         y, c->pers.netname, c->ping, status, sec / 60);
						 
        if (len >= sizeof(entry))
            continue;
		
        if (total + len >= MAX_STRING_CHARS)
            break;
		
        memcpy(buffer + total, entry, len);
		
        total += len;
        y += LAYOUT_LINE_HEIGHT;
        j++;
    }

    // add server info
    if (sv_hostname && sv_hostname->string[0]) {
        len = Q_scnprintf(entry, sizeof(entry), "xl 8 yb -37 string2 \"%s\"",
                          sv_hostname->string);
						  
        if (total + len < MAX_STRING_CHARS) {
            memcpy(buffer + total, entry, len);
            total += len;
        }
    }

    buffer[total] = 0;

    return total;
}

/*
==================
Used to update per-client scoreboard and build
global oldscores (client is NULL in the latter case).

Build Vertically
==================
*/
size_t G_BuildScoreboard_V(char *buffer, gclient_t *client, arena_t *arena)
{
    char    entry[MAX_STRING_CHARS];
    char    status[MAX_QPATH];
    char    timebuf[16];
    size_t  total, len;
    int     i, j, numranks;
    int     y, sec, eff;
    gclient_t   *ranks[MAX_CLIENTS];
    gclient_t   *c;
    time_t      t;
    struct tm   *tm;

	y = 20;		// starting point down from top of screen
	
	t = time(NULL);
    tm = localtime(&t);
	len = strftime(status, sizeof(status), "%b %e, %Y %H:%M ", tm);
	
	if (len < 1)
        strcpy(status, "???");
	
    if (!client) {
        Q_snprintf(entry, sizeof(entry),
                   "yt %d cstring2 \"Old scoreboard from %s\"", y, level.mapname);
    } else {
		Q_snprintf(entry, sizeof(entry), "yt %d cstring2 \"%s - %s\"", y, status, arena->name);
    }

	y += LAYOUT_LINE_HEIGHT * 2;
	
    total = Q_scnprintf(buffer, MAX_STRING_CHARS,
                        "xv 0 %s "
                        "yt %d "
                        "cstring \"Team %s\" "
                        "yt %d "
                        "cstring2 \"Player          Frg Rnd Mch FPH Time Ping\" ", 
						entry, y, arena->team_home.name, y + LAYOUT_LINE_HEIGHT
	);

    numranks = G_CalcArenaRanks(ranks, &arena->team_home);

	
	
    // hometeam first, add the clients sorted by rank
    y += LAYOUT_LINE_HEIGHT * 2;
    for (i = 0; i < numranks; i++) {
        c = ranks[i];

        sec = (level.framenum - c->resp.enter_framenum) / HZ;
		
        if (!sec) {
            sec = 1;
        }

        if (c->resp.score > 0) {
            j = c->resp.score + c->resp.deaths;
            eff = j ? c->resp.score * 100 / j : 100;
        } else {
            eff = 0;
        }

        if (level.framenum < 10 * 60 * HZ) {
            sprintf(timebuf, "%d:%02d", sec / 60, sec % 60);
        } else {
            sprintf(timebuf, "%d", sec / 60);
        }

        len = Q_snprintf(entry, sizeof(entry),
                         "yt %d cstring%s \"%-15s %3d %3d %3d %3d %4s %4d\"",
                         y, c == client ? "" : "2",
                         c->pers.netname, c->resp.score, c->resp.round_score, c->resp.match_score,
                         c->resp.score * 3600 / sec, timebuf, c->ping);
						 
        if (len >= sizeof(entry))
            continue;
		
        if (total + len >= MAX_STRING_CHARS)
            break;
		
        memcpy(buffer + total, entry, len);
		
        total += len;
        y += LAYOUT_LINE_HEIGHT;
    }
	
	y += LAYOUT_LINE_HEIGHT * 4;
	
	total += Q_scnprintf(buffer + total , MAX_STRING_CHARS,
                        "xv 0 %s"
                        "yt %d "
                        "cstring \"Team %s\""
                        "yt %d "
                        "cstring2 \"Player          Frg Rnd Mch FPH Time Ping\"", 
						entry, y, arena->team_away.name, y + LAYOUT_LINE_HEIGHT);
	
	numranks = G_CalcArenaRanks(ranks, &arena->team_away);

	y += LAYOUT_LINE_HEIGHT * 2;
	
    // away team second, add the clients sorted by rank
    for (i = 0; i < numranks; i++) {
        c = ranks[i];

        sec = (level.framenum - c->resp.enter_framenum) / HZ;
        if (!sec) {
            sec = 1;
        }

        if (c->resp.score > 0) {
            j = c->resp.score + c->resp.deaths;
            eff = j ? c->resp.score * 100 / j : 100;
        } else {
            eff = 0;
        }

        if (level.framenum < 10 * 60 * HZ) {
            sprintf(timebuf, "%d:%02d", sec / 60, sec % 60);
        } else {
            sprintf(timebuf, "%d", sec / 60);
        }

        len = Q_snprintf(entry, sizeof(entry),
                         "yt %d cstring%s \"%-15s %3d %3d %3d %3d %4s %4d\"",
                         y, c == client ? "" : "2",
                         c->pers.netname, c->resp.score, c->resp.round_score, c->resp.match_score,
                         c->resp.score * 3600 / sec, timebuf, c->ping);
						 
        if (len >= sizeof(entry))
            continue;

        if (total + len >= MAX_STRING_CHARS)
            break;
		
        memcpy(buffer + total, entry, len);
		
        total += len;
        y += LAYOUT_LINE_HEIGHT;
    }

	y += LAYOUT_LINE_HEIGHT * 4;
	
    // add spectators in fixed order
	total += Q_scnprintf(buffer + total , MAX_STRING_CHARS, "yt %d cstring \"-------------- Spectators ------------\"", y);
	y += LAYOUT_LINE_HEIGHT ;					
    for (i = 0, j = 0; i < game.maxclients; i++) {
        c = &game.clients[i];
		
        if (c->pers.connected != CONN_PREGAME && c->pers.connected != CONN_SPECTATOR)
            continue;
		
		if (c->pers.arena != arena)
			continue;
		
        if (c->pers.mvdspec)
            continue;

        sec = (level.framenum - c->resp.enter_framenum) / HZ;
        if (!sec) {
            sec = 1;
        }

        if (c->chase_target) {
            Q_snprintf(status, sizeof(status), "-> %.13s",
                       c->chase_target->client->pers.netname);
        } else {
            //strcpy(status, "(observing)");
			status[0] = 0;
        }

        len = Q_snprintf(entry, sizeof(entry),
                         "yt %d cstring \"%-31s %4d %4d\"",
                         y, va("%s %s", c->pers.netname, status), sec / 60, c->ping);
						 
        if (len >= sizeof(entry))
            continue;
		
        if (total + len >= MAX_STRING_CHARS)
            break;
		
        memcpy(buffer + total, entry, len);
		
        total += len;
        y += LAYOUT_LINE_HEIGHT;
        j++;
    }

    // add server info
    if (sv_hostname && sv_hostname->string[0]) {
        len = Q_scnprintf(entry, sizeof(entry), "xl 8 yb -37 string2 \"%s\"",
                          sv_hostname->string);
						  
        if (total + len < MAX_STRING_CHARS) {
            memcpy(buffer + total, entry, len);
            total += len;
        }
    }

    buffer[total] = 0;

    return total;
}

int G_CalcArenaRanks(gclient_t **ranks, arena_team_t *team) {
    int i, total;

    // sort the clients by score, then by eff
    total = 0;
    for (i = 0; i < MAX_ARENA_TEAM_PLAYERS; i++) {
		if (!team->players[i])
			continue;
		
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

void G_Centerprintf(arena_t *a, const char *fmt, ...) {
	
	va_list     argptr;
    char        string[MAX_STRING_CHARS];
    size_t      len;
	int i;
	edict_t *ent;
	
	va_start(argptr, fmt);
    len = Q_vsnprintf(string, sizeof(string), fmt, argptr);
    va_end(argptr);
	
	for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
		if (a->team_home.players[i]) {
			ent = a->team_home.players[i];
			gi.WriteByte(svc_centerprint);
			gi.WriteString(string);
			gi.unicast(ent, true);
		}
		
		if (a->team_away.players[i]) {
			ent = a->team_away.players[i];
			gi.WriteByte(svc_centerprint);
			gi.WriteString(string);
			gi.unicast(ent, true);
		}
	}
}

// see if all players are ready
qboolean G_CheckReady(arena_t *a) {
	qboolean ready_home = false;
	qboolean ready_away = false;
	
	int i;
	for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
		if (a->team_home.players[i]) {
			if (!a->team_home.players[i]->client->pers.ready) {
				ready_home = false;
				break;
			} else {
				ready_home = true;
			}
		}
	}
	
	for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
		if (a->team_away.players[i]) {
			if (!a->team_away.players[i]->client->pers.ready) {
				ready_away = false;
				break;
			} else {
				ready_away = true;
			}
		}
	}
	
	return ready_home && ready_away;
}

// match countdowns...
void G_CheckTimers(arena_t *a) {
	
	// only look once per second
	if (level.framenum % HZ == 0) {
		
		if (a->state == ARENA_STATE_COUNTDOWN) {
			int remaining = (a->round_start_frame - level.framenum) / HZ;
			switch (remaining) {
				case 10:
					G_ArenaSound(a, level.sounds.count);
					break;
			}
		}
		
		if (a->state == ARENA_STATE_TIMEOUT) {
			int remaining = (a->timein_frame - level.framenum) / HZ;
			switch (remaining) {
				case 10:
					G_ArenaSound(a, level.sounds.count);
					break;
			}
		}
	}
}

void G_ForceReady(arena_team_t *team, qboolean ready) {
	
	int i;
	for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
		if (team->players[i]) {
			team->players[i]->client->pers.ready = ready;
		}
	}
}

void G_EndMatch(arena_t *a, arena_team_t *winner) {
	
	int i;
	for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
		if (!winner->players[i])
			continue;
		
		winner->players[i]->client->resp.match_score++;
	}
	
	G_bprintf(a, PRINT_HIGH, "Match finished\n", a->current_round);
	
	a->state = ARENA_STATE_WARMUP;
	
	G_ForceReady(&a->team_home, false);
	G_ForceReady(&a->team_away, false);
}

void G_EndRound(arena_t *a, arena_team_t *winner) {
	a->round_start_frame = 0;
	G_bprintf(a, PRINT_HIGH, "Team %s won round %d/%d!\n", winner->name, a->current_round, a->round_limit);
	
	update_playercounts(a);	// for sanity
	
	int i;
	for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
		if (!winner->players[i])
			continue;
		
		winner->players[i]->client->resp.round_score++;
	}
	
	if (a->current_round == a->round_limit) {
		G_EndMatch(a, winner);
		return;
	}
	
	a->current_round++;
	
	a->state = ARENA_STATE_COUNTDOWN;
	a->countdown = (int)g_round_countdown->value;
	a->round_start_frame = level.framenum + SECS_TO_FRAMES(a->countdown);
	
	
	a->round_end_frame = 0;
	G_HideScores(a);
	G_RespawnPlayers(a);
}


void G_FreezePlayers(arena_t *a, qboolean freeze) {
	
	if (!a)
		return;
	
	pmtype_t type;
	if (freeze) {
		type = PM_FREEZE;
	} else {
		type = PM_NORMAL;
	}
		
	int i;
	for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
		if (a->team_home.players[i]) {
			a->team_home.players[i]->client->ps.pmove.pm_type = type;
		}
		
		if (a->team_away.players[i]) {
			a->team_away.players[i]->client->ps.pmove.pm_type = type;
		}
	}
}

// give the player all the items/weapons they need
void G_GiveItems(edict_t *ent) {
	
	int flags;
	flags = level.map->arenas[ent->client->pers.arena->number].weapon_flags;
	
	if (flags < 2)
		flags = ARENAWEAPON_ALL;
		
	// weapons
	if (flags & ARENAWEAPON_SHOTGUN)
		ent->client->inventory[ITEM_SHOTGUN] = 1;
	
	if (flags & ARENAWEAPON_SUPERSHOTGUN)
		ent->client->inventory[ITEM_SUPERSHOTGUN] = 1;
	
	if (flags & ARENAWEAPON_MACHINEGUN)
		ent->client->inventory[ITEM_MACHINEGUN] = 1;
	
	if (flags & ARENAWEAPON_CHAINGUN)
		ent->client->inventory[ITEM_CHAINGUN] = 1;
	
	if (flags & ARENAWEAPON_GRENADELAUNCHER)
		ent->client->inventory[ITEM_GRENADELAUNCHER] = 1;
	
	if (flags & ARENAWEAPON_HYPERBLASTER)
		ent->client->inventory[ITEM_HYPERBLASTER] = 1;
	
	if (flags & ARENAWEAPON_ROCKETLAUNCHER)
		ent->client->inventory[ITEM_ROCKETLAUNCHER] = 1;
	
	if (flags & ARENAWEAPON_RAILGUN)
		ent->client->inventory[ITEM_RAILGUN] = 1;
	
	if (flags & ARENAWEAPON_BFG)
		ent->client->inventory[ITEM_BFG] = 1;
	
	// ammo
	ent->client->inventory[ITEM_SLUGS] = 25;
	ent->client->inventory[ITEM_ROCKETS] = 30;
	ent->client->inventory[ITEM_CELLS] = 150;
	ent->client->inventory[ITEM_GRENADES] = 20;
	ent->client->inventory[ITEM_BULLETS] = 200;
	ent->client->inventory[ITEM_SHELLS] = 50;
	
	// armor
	ent->client->inventory[ITEM_ARMOR_BODY] = 110;
}


void G_JoinTeam(edict_t *ent, arena_team_type_t type) {
	
	if (!ent->client)
		return;
	
	arena_t *arena = ent->client->pers.arena;
	arena_team_t *team = FindTeam(ent, type);
	
	if (!arena) {
		gi.cprintf(ent, PRINT_HIGH, "Unknown arena\n");
		return;
	}
	
	if (!team) {
		gi.cprintf(ent, PRINT_HIGH, "Unknown team, can't join it\n");
		return;
	}
	
	// match has started, cant join
	if (arena->state >= ARENA_STATE_PLAY) {
		gi.cprintf(ent, PRINT_HIGH, "Match in progress, you can't join now\n");
		return;
	}
	
	if (ent->client->pers.team) {
		if (ent->client->pers.team->type == type) {
			G_PartTeam(ent, false);
			return;
		} else {
			G_PartTeam(ent, false);
		}
	}
	
	// add player to the team
	ent->client->pers.team = team;
	team->player_count++;

	int i;
	for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
		if (!team->players[i]) {	// free player slot, take it
			team->players[i] = ent;
			break;
		}
	}
	
	if (!team->captain) {
		team->captain = ent;
	}
	
	G_bprintf(arena, PRINT_HIGH, "%s joined team %s\n", ent->client->pers.netname, team->name);
	
	// force the skin
	G_SetSkin(ent, team->skin);
	
	// throw them into the game
	spectator_respawn(ent, CONN_SPAWNED);
}

// remove this player from whatever team they're on
void G_PartTeam(edict_t *ent, qboolean silent) {
	
	arena_team_t *oldteam;
	arena_team_t *otherteam;
	arena_t *arena;
	
	if (!ent->client)
		return;
	
	arena = ent->client->pers.arena;
	oldteam = ent->client->pers.team;
	
	if (!oldteam)
		return;
	
	oldteam->player_count--;
	if (oldteam->captain == ent) {
		oldteam->captain = 0;
	}
	
	int i;
	for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
		if (oldteam->players[i] == ent) {
			oldteam->players[i] = NULL;
			break;
		}
	}
	
	ent->client->pers.team = 0;

	if (!silent) {
		G_bprintf(ent->client->pers.arena, PRINT_HIGH, "%s left team %s\n", ent->client->pers.netname, oldteam->name);
	}
	
	// last team member, end the match
	if (oldteam->player_count == 0) {
		otherteam = (oldteam == &arena->team_home) ? &arena->team_away : &arena->team_home;
		G_EndMatch(arena, otherteam);
	}
	
	spectator_respawn(ent, CONN_SPECTATOR);
}

// give back all the ammo, health and armor for start of a round
void G_RefillInventory(edict_t *ent) {
	// ammo
	ent->client->inventory[ITEM_SLUGS] = 25;
	ent->client->inventory[ITEM_ROCKETS] = 30;
	ent->client->inventory[ITEM_CELLS] = 150;
	ent->client->inventory[ITEM_GRENADES] = 20;
	ent->client->inventory[ITEM_BULLETS] = 200;
	ent->client->inventory[ITEM_SHELLS] = 50;
	
	// armor
	ent->client->inventory[ITEM_ARMOR_BODY] = 110;
	
	// health
	ent->health = 100;
}

// respawn all players resetting their inventory
void G_RespawnPlayers(arena_t *a) {
	
	int i;
	edict_t *ent;
	
	for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
		
		ent = a->team_home.players[i];
		if (ent && ent->inuse) {
			G_RefillInventory(ent);
			spectator_respawn(ent, CONN_SPAWNED);
			G_StartingWeapon(ent);
		}
		
		ent = a->team_away.players[i];
		if (ent && ent->inuse) {
			G_RefillInventory(ent);
			spectator_respawn(ent, CONN_SPAWNED);
			G_StartingWeapon(ent);
		}
		
		a->team_home.players_alive = a->team_home.player_count;
		a->team_away.players_alive = a->team_away.player_count;
	}
}

char *G_RoundToString(arena_t *a) {
	
	static char     round_buffer[32];
	sprintf (round_buffer, "%d/%d", a->current_round, a->round_limit);

	return round_buffer;
}

void G_SetSkin(edict_t *ent, const char *skin) {
	
	if (!ent->client) {
		return;
	}
	
	edict_t *e;
	
	for (e = g_edicts + 1; e <= g_edicts + game.maxclients; e++) {	
		if (!e->inuse)
			continue;
		
		gi.WriteByte(svc_configstring);
		gi.WriteShort(CS_PLAYERSKINS + (ent - g_edicts) - 1);
		gi.WriteString(va("%s\\%s", ent->client->pers.netname, skin));
		gi.unicast(e, true);
	}
}

void G_ShowScores(arena_t *a) {
	int i;
	
	for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
		if (a->team_home.players[i]) {
			a->team_home.players[i]->client->layout = LAYOUT_SCORES;
		}
		
		if (a->team_away.players[i]) {
			a->team_away.players[i]->client->layout = LAYOUT_SCORES;
		}
	}
}

void G_HideScores(arena_t *a) {
	int i;
	
	for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
		if (a->team_home.players[i]) {
			a->team_home.players[i]->client->layout = 0;
		}
		
		if (a->team_away.players[i]) {
			a->team_away.players[i]->client->layout = 0;
		}
	}
}

void G_StartRound(arena_t *a) {
	
	a->team_home.players_alive = a->team_home.player_count;
	a->team_away.players_alive = a->team_away.player_count;
	
	a->state = ARENA_STATE_PLAY;

	G_Centerprintf(a, "Fight!");
	G_ArenaSound(a, level.sounds.secret);
}

// switches the player's gun-in-hand after spawning
void G_StartingWeapon(edict_t *ent) {
	if (!ent->client)
		return;
	
	ent->client->newweapon = FindItem("rocket launcher");
	ChangeWeapon(ent);
}

// find the team this guy is on and record the death
void G_TeamMemberDeath(edict_t *ent) {
	
	int i = 0;
	arena_team_t *team = ent->client->pers.team;
	
	if (i && team)
		return;

}

// do stuff if this arena is currently in a timeout
void G_TimeoutFrame(arena_t *a) {
	
	// expired
	if (a->timein_frame == level.framenum) {
		a->state = ARENA_STATE_PLAY;
		a->timeout_frame = 0;
		a->timein_frame = 0;
		a->timeout_caller = NULL;
		return;
	}
	
	// countdown timer
	int framesleft = a->timein_frame - level.framenum;
	if (framesleft > 0 && framesleft % SECS_TO_FRAMES(1) == 0) {
		gi.configstring(
			CS_ARENA_TIMEOUT + a->number, 
			va("Timeout: %s", G_SecsToString(FRAMES_TO_SECS(framesleft)))
		);
	}
}

const char *G_SecsToString (int seconds){
	static char     time_buffer[32];
	int				mins;

	mins = seconds / 60;
	seconds -= (mins * 60);

	sprintf (time_buffer, "%d:%.2d", mins, seconds);

	return time_buffer;
}

// 
arena_team_t *G_GetWinningTeam(arena_t *a) {
	
	if (!a)
		return NULL;
	
	if (a->team_home.players_alive == 0)
		return &(a->team_away);
	
	if (a->team_away.players_alive == 0)
		return &(a->team_home);
	
	return NULL;
}	

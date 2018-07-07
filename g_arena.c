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

// convert normal ascii string to console characters
void G_AsciiToConsole(char *out, char *in) {
	uint32_t i;
	
	for (i=0; in[i] != '\0'; i++) {
		out[i] = (char)(in[i] | 0x80);
	}
	
	out[i] = '\0';
}

static int arena_find_cl_index(gclient_t *cl) {

	int i;
	for (i = 0; i < cl->pers.arena->client_count; i++) {
		if (&cl->pers.arena->clients[i] == &cl->edict) {
			return i;
		}
	}

	return -1;
}

// find the next open slot in the clients array
static int arena_find_cl_slot(arena_t *a) {

	int i;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!&a->clients[i]) {
			return i;
		}
	}

	return 0;
}

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

// generate a name for auto-recorded demos
const char *DemoName(edict_t *ent) {
	if (!TEAM(ent))
		return "";
	
	static char name[MAX_STRING_CHARS];
	
	int32_t			i;
	size_t			namesize;
	struct tm		*ts;
	time_t			t;
	cvar_t			*hostname;
	cvar_t			*demohostname;
	char			*servername;

	hostname = gi.cvar("hostname", NULL, 0);
	servername = (hostname) ? hostname->string : "unnamed_server";
	demohostname = gi.cvar("g_demo_hostname", servername, 0);

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
			name[i] == '>' || name[i] == '<' || name[i] == '|' || name[i] == ' ')
			name[i] = '_';
	}

	return name;
}

// periodically count players to make sure none got lost
static void update_playercounts(arena_t *a) {

	int i;
	int count = 0;
	gclient_t *cl;

	for (i = 0; i < game.maxclients; i++) {
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
	
	if (ARENA(ent)->state == ARENA_STATE_WARMUP)
		G_BuildPregameScoreboard(buffer, ent->client, ARENA(ent));
	else
		G_BuildScoreboard(buffer, ent->client, ARENA(ent));

	gi.WriteByte(svc_layout);
	gi.WriteString(buffer);
	gi.unicast(ent, reliable);
}

void G_ArenaPlayerboardMessage(edict_t *ent, qboolean reliable) {
	char buffer[MAX_STRING_CHARS];

	G_BuildPlayerboard(buffer, ent->client->pers.arena);

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

	for (i = 0; i < game.maxclients; i++) {

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

	for (i = 0; i < game.maxclients; i++) {

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

/**
 * Check for things like players dropping from teams
 */
void G_CheckArenaRules(arena_t *a) {

	if (!a)
		return;

	// teams are now uneven, check if we should abort
	if (a->team_away.player_count != a->team_home.player_count && a->state > ARENA_STATE_WARMUP) {
		if (g_team_balance->value > 0 ||
			a->team_away.player_count == 0 ||
			a->team_home.player_count == 0) {

			a->current_round = a->round_limit;
			G_EndRound(a, NULL);
		}
	}
}

// check for things like state changes, start/end of rounds, timeouts, countdown clocks, etc
void G_ArenaThink(arena_t *a) {
	static qboolean foundwinner = false;

	if (!a)
		return;

	// don't waste cpu if nobody is in this arena
	if (a->client_count == 0)
		return;

	if (a->state == ARENA_STATE_TIMEOUT) {
		G_TimeoutFrame(a);
		return;
	}
	
	G_CheckIntermission(a);

	// end of round
	if (a->state == ARENA_STATE_PLAY) {

		arena_team_t *winner = G_GetWinningTeam(a);

		if (winner && !foundwinner) {
			a->round_end_frame = level.framenum
					+ SECS_TO_FRAMES((int )g_round_end_time->value);
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
		G_CheckReady(a);
		if (a->ready) {	// is everyone ready?
			a->state = ARENA_STATE_COUNTDOWN;
			a->current_round = 1;
			a->round_start_frame = level.framenum
					+ SECS_TO_FRAMES((int) g_round_countdown->value);
			a->countdown = (int) g_round_countdown->value;

			G_RespawnPlayers(a);
			G_ForceDemo(a);
		}
	}

	G_CheckTimers(a);
	G_CheckArenaRules(a);
}

// broadcast print to only members of specified arena
void G_bprintf(arena_t *arena, int level, const char *fmt, ...) {
	va_list argptr;
	char string[MAX_STRING_CHARS];
	size_t len;
	int i;
	edict_t *other;

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

/*
 ==================
 Used to update per-client scoreboard and build
 global oldscores (client is NULL in the latter case).

 Build horizontally
 ==================
 */
size_t G_BuildScoreboard_H(char *buffer, gclient_t *client, arena_t *arena) {
	char entry[MAX_STRING_CHARS];
	char status[MAX_QPATH];
	char timebuf[16];
	size_t total, len;
	int i, j, numranks;
	int x, y, sec;
	//int eff;
	gclient_t *ranks[MAX_CLIENTS];
	gclient_t *c;
	time_t t;
	struct tm *tm;

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
		Q_snprintf(entry, sizeof(entry), "yt %d cstring2 \"%s - %s\"", y,
				status, arena->name);
	}

	y += LAYOUT_LINE_HEIGHT * 6;
	x = LAYOUT_CHAR_WIDTH * -30;

	total = Q_scnprintf(buffer, MAX_STRING_CHARS, "xv 0 %s "
			"xv %d "
			"yt %d "
			"cstring \"Team %s\" "
			"xv 0 yt %d cstring \"VS.\" "
			"xv %d "
			"yt %d "
			"cstring2 \"Player          Frg Rnd Mch  Rdy Time Ping\" ", entry,
			x, y, arena->team_home.name, y, x, y + LAYOUT_LINE_HEIGHT * 2);

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
			//eff = j ? c->resp.score * 100 / j : 100;
		} else {
			//eff = 0;
		}

		if (level.framenum < 10 * 60 * HZ) {
			sprintf(timebuf, "%d:%02d", sec / 60, sec % 60);
		} else {
			sprintf(timebuf, "%d", sec / 60);
		}

		len = Q_snprintf(entry, sizeof(entry),
				"yt %d cstring \"%-15s %3d %3d %3d %4s %4s %4d\"", y,
				c->pers.netname, c->resp.score, c->resp.round_score,
				c->resp.match_score, c->pers.ready ? "*" : " ", timebuf,
				c->ping);

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
	total += Q_scnprintf(buffer + total, MAX_STRING_CHARS, "xv %d "
			"yt %d "
			"cstring \"Team %s\""
			"yt %d "
			"cstring2 \"Player          Frg Rnd Mch  Rdy Time Ping\"", x, y,
			arena->team_away.name, y + LAYOUT_LINE_HEIGHT * 2);

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
			//eff = j ? c->resp.score * 100 / j : 100;
		} else {
			//eff = 0;
		}

		if (level.framenum < 10 * 60 * HZ) {
			sprintf(timebuf, "%d:%02d", sec / 60, sec % 60);
		} else {
			sprintf(timebuf, "%d", sec / 60);
		}

		len = Q_snprintf(entry, sizeof(entry),
				"yt %d cstring \"%-15s %3d %3d %3d %4s %4s %4d\"", y,
				c->pers.netname, c->resp.score, c->resp.round_score,
				c->resp.match_score, c->pers.ready ? "*" : " ", timebuf,
				c->ping);

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
	total += Q_scnprintf(buffer + total, MAX_STRING_CHARS,
			"xv 0 yt %d cstring \"----- Spectators -----\"", y);
	y += LAYOUT_LINE_HEIGHT;
	for (i = 0, j = 0; i < game.maxclients; i++) {
		c = &game.clients[i];

		if (c->pers.connected != CONN_PREGAME
				&& c->pers.connected != CONN_SPECTATOR)
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

		len = Q_snprintf(entry, sizeof(entry), "yt %d cstring \"%s:%d %s %d\"",
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
size_t G_BuildScoreboard(char *buffer, gclient_t *client, arena_t *arena) {
	char entry[MAX_STRING_CHARS];
	char status[MAX_QPATH];
	char timebuf[16];
	size_t total, len;
	int i, j, numranks;
	int y, sec;
	gclient_t *ranks[MAX_CLIENTS];
	gclient_t *c;
	time_t t;
	struct tm *tm;

	// starting point down from top of screen
	y = 20;

	// Build time string
	t = time(NULL);
	tm = localtime(&t);
	len = strftime(status, sizeof(status), "%b %e, %Y %H:%M ", tm);

	if (len < 1)
		strcpy(status, "???");

	if (!client) {
		Q_snprintf(entry, sizeof(entry),
				"yt %d cstring2 \"Old scoreboard from %s on %s\"", y, arena->name, status);
	} else {
		Q_snprintf(entry, sizeof(entry), "yt %d cstring2 \"%s - %s\"", y,
				status, arena->name);
	}

	// move down 6 lines
	y += LAYOUT_LINE_HEIGHT * 6;

	total = Q_scnprintf(buffer, MAX_STRING_CHARS, "xv 0 %s "
			"yt %d "
			"cstring \"Team %s\" "
			"yt %d "
			"cstring2 \"Name            Frg Rnd Mch FPH Time Ping\" ", entry, y,
			arena->team_home.name, y + LAYOUT_LINE_HEIGHT);

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
			//eff = j ? c->resp.score * 100 / j : 100;
		} else {
			//eff = 0;
		}

		if (level.framenum < 10 * 60 * HZ) {
			sprintf(timebuf, "%d:%02d", sec / 60, sec % 60);
		} else {
			sprintf(timebuf, "%d", sec / 60);
		}

		len = Q_snprintf(entry, sizeof(entry),
				"yt %d cstring \"%-15s %3d %3d %3d %3d %4s %4d\"", y,
				c->pers.netname, c->resp.score,
				c->resp.round_score, c->resp.match_score,
				c->resp.score * 3600 / sec, timebuf, c->ping);

		if (len >= sizeof(entry))
			continue;

		if (total + len >= MAX_STRING_CHARS)
			break;

		memcpy(buffer + total, entry, len);

		total += len;
		y += LAYOUT_LINE_HEIGHT;
	}

	// skip 4 lines
	y += LAYOUT_LINE_HEIGHT * 4;

	total += Q_scnprintf(buffer + total, MAX_STRING_CHARS, "xv 0 %s"
			"yt %d "
			"cstring \"Team %s\""
			"yt %d "
			"cstring2 \"Name            Frg Rnd Mch FPH Time Ping\"", entry, y,
			arena->team_away.name, y + LAYOUT_LINE_HEIGHT);

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
			//eff = j ? c->resp.score * 100 / j : 100;
		} else {
			//eff = 0;
		}

		if (level.framenum < 10 * 60 * HZ) {
			sprintf(timebuf, "%d:%02d", sec / 60, sec % 60);
		} else {
			sprintf(timebuf, "%d", sec / 60);
		}

		len = Q_snprintf(entry, sizeof(entry),
				"yt %d cstring \"%-15s %3d %3d %3d %3d %4s %4d\"", y,
				c->pers.netname, c->resp.score,
				c->resp.round_score, c->resp.match_score,
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
	total += Q_scnprintf(buffer + total, MAX_STRING_CHARS,
			"yt %d cstring2 \"---- Spectators ----\"", y);
	y += LAYOUT_LINE_HEIGHT;
	for (i = 0, j = 0; i < game.maxclients; i++) {
		c = &game.clients[i];

		if (c->pers.connected != CONN_PREGAME
				&& c->pers.connected != CONN_SPECTATOR)
			continue;

		if (c->pers.arena != arena)
			continue;

		if (c->pers.mvdspec)
			continue;

		sec = (level.framenum - c->resp.enter_framenum) / HZ;
		if (!sec) {
			sec = 1;
		}

		// spec is following someone, show who
		if (c->chase_target) {
			Q_snprintf(status, sizeof(status), "-> %.13s",
					c->chase_target->client->pers.netname);
		} else {
			status[0] = 0;
		}

		len = Q_snprintf(entry, sizeof(entry),
				"yt %d cstring \"%s\"", y,
				va("%s:%d %s", c->pers.netname, c->ping, status));

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
		len = Q_scnprintf(entry, sizeof(entry), "xl 8 yb -37 string2 \"%s - %s %s\"",
				sv_hostname->string,
				GAMEVERSION,
				OPENRA2_VERSION
		);

		if (total + len < MAX_STRING_CHARS) {
			memcpy(buffer + total, entry, len);
			total += len;
		}
	}

	buffer[total] = 0;

	return total;
}

size_t G_BuildPregameScoreboard(char *buffer, gclient_t *client, arena_t *arena) {
	char entry[MAX_STRING_CHARS];
	char status[MAX_QPATH];
	char timebuf[16];
	size_t total, len;
	int i, j, numranks;
	int y, sec;
	gclient_t *ranks[MAX_CLIENTS];
	gclient_t *c;
	time_t t;
	struct tm *tm;

	char bracketopen[2];
	char bracketclosed[2];
	
	G_AsciiToConsole(bracketopen, "[");
	G_AsciiToConsole(bracketclosed, "]");
	
	// starting point down from top of screen
	y = 20;

	// Build time string
	t = time(NULL);
	tm = localtime(&t);
	len = strftime(status, sizeof(status), "%b %e, %Y %H:%M ", tm);

	if (len < 1)
		strcpy(status, "???");

	if (!client) {
		Q_snprintf(entry, sizeof(entry),
				"yt %d cstring2 \"Old scoreboard from %s on %s\"", y, arena->name, status);
	} else {
		Q_snprintf(entry, sizeof(entry), "yt %d cstring2 \"%s - %s\"", y,
				status, arena->name);
	}

	// move down 6 lines
	y += LAYOUT_LINE_HEIGHT * 6;

	total = Q_scnprintf(buffer, MAX_STRING_CHARS, "xv 0 %s "
			"yt %d "
			"cstring \"Team %s\" "
			"yt %d "
			"cstring2 \"Name                            Time Ping\" ", entry, y,
			arena->team_home.name, y + LAYOUT_LINE_HEIGHT);

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
			//eff = j ? c->resp.score * 100 / j : 100;
		} else {
			//eff = 0;
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

		if (len >= sizeof(entry))
			continue;

		if (total + len >= MAX_STRING_CHARS)
			break;

		memcpy(buffer + total, entry, len);

		total += len;
		y += LAYOUT_LINE_HEIGHT;
	}

	// skip 4 lines
	y += LAYOUT_LINE_HEIGHT * 4;

	total += Q_scnprintf(buffer + total, MAX_STRING_CHARS, "xv 0 %s"
			"yt %d "
			"cstring \"Team %s\""
			"yt %d "
			"cstring2 \"Name                            Time Ping\"", entry, y,
			arena->team_away.name, y + LAYOUT_LINE_HEIGHT);

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
			//eff = j ? c->resp.score * 100 / j : 100;
		} else {
			//eff = 0;
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
	total += Q_scnprintf(buffer + total, MAX_STRING_CHARS,
			"yt %d cstring2 \"---- Spectators ----\"", y);
	y += LAYOUT_LINE_HEIGHT;
	for (i = 0, j = 0; i < game.maxclients; i++) {
		c = &game.clients[i];

		if (c->pers.connected != CONN_PREGAME
				&& c->pers.connected != CONN_SPECTATOR)
			continue;

		if (c->pers.arena != arena)
			continue;

		if (c->pers.mvdspec)
			continue;

		sec = (level.framenum - c->resp.enter_framenum) / HZ;
		if (!sec) {
			sec = 1;
		}

		// spec is following someone, show who
		if (c->chase_target) {
			Q_snprintf(status, sizeof(status), "-> %.13s",
					c->chase_target->client->pers.netname);
		} else {
			status[0] = 0;
		}

		len = Q_snprintf(entry, sizeof(entry),
				"yt %d cstring \"%s\"", y,
				va("%s:%d %s", c->pers.netname, c->ping, status));

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
		len = Q_scnprintf(entry, sizeof(entry), "xl 8 yb -37 string2 \"%s - %s %s\"",
				sv_hostname->string,
				GAMEVERSION,
				OPENRA2_VERSION
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

	// starting point down from top of screen
	y = 20;

	// Build time string
	t = time(NULL);
	len = strftime(status, sizeof(status), "%b %e, %Y %H:%M ", (struct tm *) localtime(&t));

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

		if (!c || c->pers.connected <= CONN_CONNECTED)
			continue;

		a = c->pers.arena;

		len = Q_snprintf(entry, sizeof(entry),
				"yt %d cstring \"%-15s %-20s %4d\"", y,
				c->pers.netname, va("%d:%s", a->number, a->name), c->ping);

		if (len >= sizeof(entry))
			continue;

		if (total + len >= MAX_STRING_CHARS)
			break;

		memcpy(buffer + total, entry, len);

		total += len;
		y += LAYOUT_LINE_HEIGHT;
	}

	// add server info
	if (sv_hostname && sv_hostname->string[0]) {
		len = Q_scnprintf(entry, sizeof(entry), "xl 8 yb -37 string2 \"%s - %s %s\"",
				sv_hostname->string,
				GAMEVERSION,
				OPENRA2_VERSION
		);

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

	va_list argptr;
	char string[MAX_STRING_CHARS];
	size_t len;
	int i;
	edict_t *ent;

	va_start(argptr, fmt);
	len = Q_vsnprintf(string, sizeof(string), fmt, argptr);
	va_end(argptr);

	if (len == 0) {
		return;
	}

	for (i = 0; i < MAX_ARENA_TEAM_PLAYERS; i++) {
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

// move client to a different arena
void G_ChangeArena(gclient_t *cl, arena_t *arena) {

	int index = 0;

	// leave the old arena
	if (cl->pers.arena) {
		index = arena_find_cl_index(cl);

		cl->pers.arena->clients[index] = NULL;
		cl->pers.arena->client_count--;

		G_PartTeam(cl->edict, true);

		if (arena) {
			G_bprintf(cl->pers.arena, PRINT_HIGH, "%s left this arena\n",
					cl->pers.netname);
		}
	}

	if (!arena) {
		return;
	}

	index = arena_find_cl_slot(arena);

	arena->client_count++;
	arena->clients[index] = cl->edict;
	cl->pers.arena = arena;

	cl->pers.connected = CONN_SPECTATOR;
	cl->pers.ready = false;

	PutClientInServer(cl->edict);
	G_ArenaSound(arena, level.sounds.teleport);

	G_bprintf(arena, PRINT_HIGH, "%s joined this arena\n", cl->pers.netname);

	// hold in place briefly
	cl->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
	cl->ps.pmove.pm_time = 14;

	cl->respawn_framenum = level.framenum;
}

/**
 * Check if all players are ready and set the next time we should check
 *
 */
void G_CheckReady(arena_t *a) {
	int i;
	
	if (level.framenum != a->ready_think_frame) {
		return;
	}
	
	a->ready_think_frame = SECS_TO_FRAMES(2) + level.framenum;
	
	// need players on both teams
	if (a->team_home.player_count == 0 || a->team_away.player_count == 0) {
		a->ready = false;
		return;
	}
	
	for (i = 0; i < MAX_ARENA_TEAM_PLAYERS; i++) {
		if (a->team_home.players[i]) {
			if (!a->team_home.players[i]->client->pers.ready) {
				a->ready = false;
				return;
			}
		}

		if (a->team_away.players[i]) {
			if (!a->team_away.players[i]->client->pers.ready) {
				a->ready = false;
				return;
			}
		}
	}

	if (g_team_balance->value > 0 && a->team_home.player_count != a->team_away.player_count) {
		a->ready = false;
		return;
	}

	a->ready = true;
}

// match countdowns...
void G_CheckTimers(arena_t *a) {
	
	if (!(a->timer_last_frame + SECS_TO_FRAMES(1) <= level.framenum)) {
		return;
	}
	
	a->timer_last_frame = level.framenum;
	
	uint32_t remaining;
	if (a->state == ARENA_STATE_COUNTDOWN) {
		remaining = (a->round_start_frame - level.framenum) / HZ;
		switch (remaining) {
		case 10:
			G_ArenaSound(a, level.sounds.count);
			break;
		}
	}

	if (a->state == ARENA_STATE_TIMEOUT) {
		remaining = (a->timein_frame - level.framenum) / HZ;
		switch (remaining) {
		case 10:
			G_ArenaSound(a, level.sounds.count);
			break;
		}
	}
}


/**
 * Force the ready state for all players in an arena
 *
 */
void G_ForceReady(arena_team_t *team, qboolean ready) {

	int i;
	for (i = 0; i < MAX_ARENA_TEAM_PLAYERS; i++) {
		if (team->players[i]) {
			team->players[i]->client->pers.ready = ready;
		}
	}
}

void G_ForceDemo(arena_t *arena) {
	uint32_t i;
	
	if (!g_demo->value) {
		return;
	}
	
	if (arena->recording) {
		for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
			
			if (arena->team_home.players[i]) {
				G_StuffText(arena->team_home.players[i], "stop\n");
			}
			
			if (arena->team_away.players[i]) {
				G_StuffText(arena->team_away.players[i], "stop\n");
			}
		}
		
		arena->recording = qfalse;
		
	} else {
		for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
			if (arena->team_home.players[i]) {
				G_StuffText(arena->team_home.players[i], va("record \"%s\"\n", DemoName(arena->team_home.players[i])));
			}
			
			if (arena->team_away.players[i]) {
				G_StuffText(arena->team_away.players[i], va("record \"%s\"\n", DemoName(arena->team_away.players[i])));
			}
		}
		
		arena->recording = qtrue;
	}
}

void G_ForceScreenshot(arena_t *arena) {
	if (!g_screenshot->value) {
		return;
	}
	
	uint32_t i;
	for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
		if (arena->team_home.players[i]) {
			G_StuffText(arena->team_home.players[i], "screenshot\n");
		}
		
		if (arena->team_away.players[i]) {
			G_StuffText(arena->team_away.players[i], "screenshot\n");
		}
	}
}

/**
 * Reset after match is finished
 *
 */
void G_EndMatch(arena_t *a, arena_team_t *winner) {

	int i;
	if (winner) {
		for (i = 0; i < MAX_ARENA_TEAM_PLAYERS; i++) {
			if (!winner->players[i])
				continue;

			winner->players[i]->client->resp.match_score++;
		}
	}

	// save the current scoreboard as an oldscore
	G_BuildScoreboard(a->oldscores, NULL, a);

	G_bprintf(a, PRINT_HIGH, "Match finished\n");

	a->state = ARENA_STATE_WARMUP;
	a->ready = false;

	// un-ready everyone
	G_ForceReady(&a->team_home, false);
	G_ForceReady(&a->team_away, false);
	
	G_ForceScreenshot(a);
	G_ForceDemo(a);
}


/**
 * All players on a particular team have been fragged
 *
 */
void G_EndRound(arena_t *a, arena_team_t *winner) {

	int i;
	a->round_start_frame = 0;

	if (winner) {
		G_bprintf(a, PRINT_HIGH, "Team %s won round %d/%d!\n", winner->name,
				a->current_round, a->round_limit);

		update_playercounts(a);	// for sanity

		for (i = 0; i < MAX_ARENA_TEAM_PLAYERS; i++) {
			if (!winner->players[i])
				continue;

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
	a->round_end_frame = 0;

	G_HideScores(a);
	G_RespawnPlayers(a);
}


/**
 * Lock players in place during timeouts
 *
 */
void G_FreezePlayers(arena_t *a, qboolean freeze) {

	if (!a)
		return;

	int i;
	pmtype_t type;

	if (freeze) {
		type = PM_FREEZE;
	} else {
		type = PM_NORMAL;
	}

	for (i = 0; i < MAX_ARENA_TEAM_PLAYERS; i++) {
		if (a->team_home.players[i]) {
			a->team_home.players[i]->client->ps.pmove.pm_type = type;
		}

		if (a->team_away.players[i]) {
			a->team_away.players[i]->client->ps.pmove.pm_type = type;
		}
	}
}

/**
 * Give the player all the items/weapons they need. Called in PutClientInServer()
 *
 */
void G_GiveItems(edict_t *ent) {

	if (!ent->client)
		return;
	
	int flags;
	arena_t *a;
	a = ARENA(ent);
	
	flags = a->weapon_flags;

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
	ent->client->inventory[ITEM_SLUGS] = a->slugs;
	ent->client->inventory[ITEM_ROCKETS] = a->rockets;
	ent->client->inventory[ITEM_CELLS] = a->cells;
	ent->client->inventory[ITEM_GRENADES] = a->grenades;
	ent->client->inventory[ITEM_BULLETS] = a->bullets;
	ent->client->inventory[ITEM_SHELLS] = a->shells;

	// armor
	ent->client->inventory[ITEM_ARMOR_BODY] = a->armor;
	
	// health
	ent->health = a->health;
}


/**
 * Add player to a team
 *
 */
void G_JoinTeam(edict_t *ent, arena_team_type_t type, qboolean forced) {

	if (!ent->client)
		return;

	arena_t *arena = ent->client->pers.arena;
	arena_team_t *team = FindTeam(ent, type);

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
	for (i = 0; i < MAX_ARENA_TEAM_PLAYERS; i++) {
		if (!team->players[i]) {	// free player slot, take it
			team->players[i] = ent;
			break;
		}
	}

	if (!team->captain) {
		team->captain = ent;
	}

	G_bprintf(arena, PRINT_HIGH, "%s joined team %s\n", NAME(ent), team->name);

	// force the skin
	G_SetSkin(ent, team->skin);

	// throw them into the game
	spectator_respawn(ent, CONN_SPAWNED);
}


/**
 * Remove this player from whatever team they're on
 *
 */
void G_PartTeam(edict_t *ent, qboolean silent) {

	arena_team_t *oldteam;
	arena_team_t *otherteam;
	arena_t *arena;
	int i;

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

	for (i = 0; i < MAX_ARENA_TEAM_PLAYERS; i++) {
		if (oldteam->players[i] == ent) {
			oldteam->players[i] = NULL;
			break;
		}
	}

	ent->client->pers.team = 0;

	if (!silent) {
		G_bprintf(ARENA(ent), PRINT_HIGH, "%s left team %s\n",
				NAME(ent), oldteam->name);
	}

	ent->client->pers.ready = false;

	// last team member, end the match
	if (oldteam->player_count == 0 && arena->state == ARENA_STATE_PLAY) {
		otherteam =
				(oldteam == &arena->team_home) ?
						&arena->team_away : &arena->team_home;
		G_EndMatch(arena, otherteam);
	}

	spectator_respawn(ent, CONN_SPECTATOR);
}


/**
 * Give back all the ammo, health and armor for start of a round
 *
 */
void G_RefillInventory(edict_t *ent) {
	
	arena_t *a;
	a = ARENA(ent);
	
	// ammo
	ent->client->inventory[ITEM_SLUGS] = a->slugs;
	ent->client->inventory[ITEM_ROCKETS] = a->rockets;
	ent->client->inventory[ITEM_CELLS] = a->cells;
	ent->client->inventory[ITEM_GRENADES] = a->grenades;
	ent->client->inventory[ITEM_BULLETS] = a->bullets;
	ent->client->inventory[ITEM_SHELLS] = a->shells;

	// armor
	ent->client->inventory[ITEM_ARMOR_BODY] = a->armor;

	// health
	ent->health = a->health;
}

/**
 * Respawn all players resetting their inventory
 *
 */
void G_RespawnPlayers(arena_t *a) {

	int i;
	edict_t *ent;

	for (i = 0; i < MAX_ARENA_TEAM_PLAYERS; i++) {
		
		ent = a->team_home.players[i];
		if (ent && ent->inuse) {
			G_RefillInventory(ent);
			spectator_respawn(ent, CONN_SPAWNED);
		}

		ent = a->team_away.players[i];
		if (ent && ent->inuse) {
			G_RefillInventory(ent);
			spectator_respawn(ent, CONN_SPAWNED);
		}

		a->team_home.players_alive = a->team_home.player_count;
		a->team_away.players_alive = a->team_away.player_count;
	}
}

char *G_RoundToString(arena_t *a) {

	static char round_buffer[32];
	sprintf(round_buffer, "%d/%d", a->current_round, a->round_limit);

	return round_buffer;
}


/**
 * Force a particular skin on a player
 *
 */
void G_SetSkin(edict_t *ent, const char *skin) {

	if (!ent->client) {
		return;
	}

	G_StuffText(ent, va("set skin %s", skin));
}


/**
 * Display scores layout to all arena players
 *
 */
void G_ShowScores(arena_t *a) {
	int i;

	for (i = 0; i < MAX_ARENA_TEAM_PLAYERS; i++) {
		if (a->team_home.players[i]) {
			a->team_home.players[i]->client->layout = LAYOUT_SCORES;
		}

		if (a->team_away.players[i]) {
			a->team_away.players[i]->client->layout = LAYOUT_SCORES;
		}
	}
}


/**
 * Hide scores layout from players
 *
 */
void G_HideScores(arena_t *a) {
	int i;

	for (i = 0; i < MAX_ARENA_TEAM_PLAYERS; i++) {
		if (a->team_home.players[i]) {
			a->team_home.players[i]->client->layout = 0;
		}

		if (a->team_away.players[i]) {
			a->team_away.players[i]->client->layout = 0;
		}
	}
}


/**
 * Begin a round
 *
 */
void G_StartRound(arena_t *a) {

	a->team_home.players_alive = a->team_home.player_count;
	a->team_away.players_alive = a->team_away.player_count;

	a->state = ARENA_STATE_PLAY;

	G_Centerprintf(a, "Fight!");
	G_ArenaSound(a, level.sounds.secret);
}


/**
 * Switches the player's gun-in-hand after spawning
 *
 */
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
				va("Timeout: %s", G_SecsToString(FRAMES_TO_SECS(framesleft))));
	}
}


/**
 * Get time string (mm:ss) from seconds
 *
 */
const char *G_SecsToString(int seconds) {
	static char time_buffer[32];
	int mins;

	mins = seconds / 60;
	seconds -= (mins * 60);

	sprintf(time_buffer, "%d:%.2d", mins, seconds);

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

/**
 * Arena settings can be overridden by files in the mapcfg folder. Those settings (if present)
 * get applied to the level structure here.
 *
 */
void G_MergeArenaSettings(arena_t *a, arena_entry_t *m) {
	if (!a)
		return;

	// for maps not in the list, there will be no arenas listed
	// just inject cvar defaults
	if (!m) {
		a->damage_flags = (int) g_damage_flags->value;
		a->weapon_flags = (int) g_weapon_flags->value;
		a->round_limit = (int) g_round_limit->value;
		a->health = (int) g_health_start->value;
		a->armor = (int) g_armor_start->value;
		a->slugs = (int) g_ammo_slugs->value;
		a->rockets = (int) g_ammo_rockets->value;
		a->cells = (int) g_ammo_cells->value;
		a->grenades = (int) g_ammo_grenades->value;
		a->bullets = (int) g_ammo_bullets->value;
		a->shells = (int) g_ammo_shells->value;
		return;
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
	
	if (m->slugs) {
		a->slugs = m->slugs;
	} else {
		a->slugs = (int) g_ammo_slugs->value;
	}
	
	if (m->rockets) {
		a->rockets = m->rockets;
	} else {
		a->rockets = (int) g_ammo_rockets->value;
	}
	
	if (m->cells) {
		a->cells = m->cells;
	} else {
		a->cells = (int) g_ammo_cells->value;
	}
	
	if (m->grenades) {
		a->grenades = m->grenades;
	} else {
		a->grenades = (int) g_ammo_grenades->value;
	}
	
	if (m->bullets) {
		a->bullets = m->bullets;
	} else {
		a->bullets = (int) g_ammo_bullets->value;
	}
	
	if (m->shells) {
		a->shells = m->shells;
	} else {
		a->shells = (int) g_ammo_shells->value;
	}
}

qboolean G_Teammates(edict_t *p1, edict_t *p2) {
	if (!(p1->client && p2->client)) {
		return qfalse;
	}
	return p1->client->pers.team == p2->client->pers.team;
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
			}

			if (Q_strcasecmp(token, "damage") == 0 && inarena) {
				entry[arena_num].damage_flags = atoi(COM_Parse(&fp_data));
			}

			if (Q_strcasecmp(token, "weapons") == 0 && inarena) {
				entry[arena_num].weapon_flags = atoi(COM_Parse(&fp_data));
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
				entry[arena_num].slugs = atoi(COM_Parse(&fp_data));
			}
			
			if (Q_strcasecmp(token, "rockets") == 0 && inarena) {
				entry[arena_num].rockets = atoi(COM_Parse(&fp_data));
			}
			
			if (Q_strcasecmp(token, "cells") == 0 && inarena) {
				entry[arena_num].cells = atoi(COM_Parse(&fp_data));
			}
			
			if (Q_strcasecmp(token, "grenades") == 0 && inarena) {
				entry[arena_num].grenades = atoi(COM_Parse(&fp_data));
			}
			
			if (Q_strcasecmp(token, "bullets") == 0 && inarena) {
				entry[arena_num].bullets = atoi(COM_Parse(&fp_data));
			}
			
			if (Q_strcasecmp(token, "shells") == 0 && inarena) {
				entry[arena_num].shells = atoi(COM_Parse(&fp_data));
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

	if (!ent)
		return;

	if (!ent->client)
		return;

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

void G_ResetArena(arena_t *a) {
	gi.dprintf("Resetting Arena %d (%s)\n", a->number, a->name);
	uint8_t i;
	edict_t *player;
	
	a->intermission_framenum = 0;
	a->intermission_exit = 0;
	a->state = ARENA_STATE_WARMUP;
	
	// respawn all players
    for (i = 0; i < MAX_ARENA_TEAM_PLAYERS; i++) {
		if (a->team_home.players[i]) {
			player = a->team_home.players[i];
			memset(&player->client->resp, 0, sizeof(player->client->resp));
			memset(&player->client->level.vote, 0, sizeof(player->client->level.vote));
			player->movetype = MOVETYPE_NOCLIP; // don't leave a body
			if (g_team_reset->value) {
				G_PartTeam(player, true);
			} else {
				respawn(player);
			}
		}
		
		if (a->team_away.players[i]) {
			player = a->team_away.players[i];
			memset(&player->client->resp, 0, sizeof(player->client->resp));
			memset(&player->client->level.vote, 0, sizeof(player->client->level.vote));
			player->movetype = MOVETYPE_NOCLIP; // don't leave a body
			if (g_team_reset->value) {
				G_PartTeam(player, true);
			} else {
				respawn(player);
			}
		}
    }
}

void G_CheckIntermission(arena_t *a) {
	if (a->intermission_exit) {
		if (level.framenum > a->intermission_exit + 5) {
			G_ResetArena(a); // in case gamemap failed
		}
	} else if (a->intermission_framenum) {
		int32_t delta, exit_delta;
		exit_delta = SECS_TO_FRAMES(g_intermission_time->value);

		clamp(exit_delta, SECS_TO_FRAMES(5), SECS_TO_FRAMES(120));

		delta = level.framenum - a->intermission_framenum;
		if (delta == SECS_TO_FRAMES(1)) {
			if (rand_byte() > 127) {
				G_StartSound(level.sounds.xian);
			} else {
				G_StartSound(level.sounds.makron);
			}
		} else if (delta == exit_delta) {
			G_ResetArena(a);
		} /*else if (delta % (5 * HZ) == 0) {
			delta /= 5 * HZ;
			if (level.numscores && (delta & 1)) {
				HighScoresMessage();
			} else {
				for (i = 0, ent = &g_edicts[1]; i < game.maxclients;
						i++, ent++) {
					if (ent->client->pers.connected > CONN_CONNECTED) {
						G_ArenaScoreboardMessage(ent, qtrue);
					}
				}
			}
		}*/
	}
}

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

/**
 * Find the index in the client array for the client's current arena
 */
static int arena_find_cl_index(gclient_t *cl) {

	int i;
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
	for (i = 0; i < ARENA(ent)->spectator_count; i++) {
		if (ARENA(ent)->spectators[i] == ent) {
			return i;
		}
	}

	return -1;
}

// find the next open slot in the clients array
static int arena_find_cl_slot(arena_t *a) {

	int i;
	for (i = 0; i < game.maxclients; i++) {
		if (!a->clients[i]) {
			return i;
		}
	}

	return -1;
}

// find the next open slot in the spectator array
static int arena_find_sp_slot(arena_t *a) {

	int i;
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
	if (!ent->client)
		return;

	if (arena_find_sp_index(ent) == -1) {
		idx = arena_find_sp_slot(ARENA(ent));
		ARENA(ent)->spectators[idx] = ent;
		ARENA(ent)->spectator_count++;
	}
}

/**
 * Leave the spectator "team" for the player's arena
 */
void G_SpectatorsPart(edict_t *ent) {

	int8_t idx;

	if (!ent->client)
		return;

	idx = arena_find_sp_index(ent);

	if (idx >= 0) {
		ARENA(ent)->spectators[idx] = NULL;
		ARENA(ent)->spectator_count--;
	}
}

static arena_team_t *FindTeam(arena_t *a, arena_team_type_t type) {

	uint8_t i;
	//arena_t *a = ARENA(ent);

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

	gi.WriteByte(SVC_LAYOUT);
	gi.WriteString(buffer);
	gi.unicast(ent, reliable);
}

void G_ArenaPlayerboardMessage(edict_t *ent, qboolean reliable) {
	char buffer[MAX_STRING_CHARS];

	G_BuildPlayerboard(buffer, ARENA(ent));

	gi.WriteByte(SVC_LAYOUT);
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

	uint8_t previous, i;

	if (!a) {
		return;
	}

	// watch for players disconnecting mid match
	if (a->state > ARENA_STATE_WARMUP && a->state < ARENA_STATE_INTERMISSION) {
		previous = a->teams[0].player_count;

		for (i=1; i<a->team_count; i++) {
			if ((a->teams[i].player_count != previous && g_team_balance->value > 0) || a->teams[i].player_count == 0) {
				a->current_round = a->round_limit;
				G_EndRound(a, NULL);
				return;
			}

			previous = a->teams[i].player_count;
		}
	}

	// round intermission just expired
	if (a->round_intermission_start && level.framenum == a->round_intermission_end) {
		G_EndRoundIntermission(a);
		return;
	}

	// round timelimit hit
	if (a->round_end_frame > 0 && a->round_end_frame == a->round_frame) {
		G_BeginRoundIntermission(a);
	}
}

/**
 * Run once per frame for each arena in the map. Keeps track of state changes,
 * round/match beginning and ending, timeouts, countdowns, votes, etc.
 *
 */
void G_ArenaThink(arena_t *a)
{
	if (!a) {
		return;
	}

	// don't waste cpu if nobody is in this arena
	if (a->client_count == 0) {
		return;
	}

	if (a->state == ARENA_STATE_TIMEOUT) {
		G_TimeoutFrame(a);
		return;
	}
	
	G_CheckIntermission(a);
	
	// end of round
	if (ROUNDOVER(a)) {
		G_BeginRoundIntermission(a);
	}

	// countdown to start
	if (a->state < ARENA_STATE_PLAY && a->round_start_frame) {
		int framesleft = a->round_start_frame - level.framenum;

		if (framesleft > 0 && framesleft % SECS_TO_FRAMES(1) == 0) {
			a->countdown = FRAMES_TO_SECS(framesleft);
		} else if (framesleft == 0) {
			G_StartRound(a);
			return;
		}
	}

	/*
	if (a->state == ARENA_STATE_COUNTDOWN) {
		if (level.framenum - a->round_start_frame == 20) {
			G_ArenaSound(a, level.sounds.round[a->current_round]);
		}
	}
	*/

	// pregame
	if (a->state == ARENA_STATE_WARMUP) {
		if (a->ready) {	// is everyone ready?
			a->state = ARENA_STATE_COUNTDOWN;
			
			G_ClearRoundInfo(a);
			
			a->round_start_frame = level.framenum
					+ SECS_TO_FRAMES((int) g_round_countdown->value);
			a->round_frame = a->round_start_frame;
			a->match_frame = a->round_start_frame;

			a->countdown = (int) g_round_countdown->value;

			G_RespawnPlayers(a);
			G_ForceDemo(a);
		}
	}

	G_CheckTimers(a);
	G_UpdateArenaVote(a);
	G_CheckArenaRules(a);

	if (a->state > ARENA_STATE_WARMUP) {
		a->round_frame++;
		a->match_frame++;
	}
}

// Stuff that needs to be reset between rounds
void G_ClearRoundInfo(arena_t *a) {
	uint8_t i;

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

	for (i=0; i<game.maxclients; i++) {
		ent = arena->clients[i];
		if (!ent)
			continue;

		if (!ent->client)
			continue;

		gi.WriteByte(SVC_CONFIGSTRING);
		gi.WriteShort(index);
		gi.WriteString(string);
		gi.unicast(ent, qtrue);
	}
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

		if (arena != ARENA(other))
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

 Build Vertically
 ==================
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

	// starting point down from top of screen
	y = 20;

	// Build time string
	t = time(NULL);
	tm = localtime(&t);
	len = strftime(status, sizeof(status), DATE_FORMAT, tm);

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

	total = 0;
	for (k=0; k<arena->team_count; k++) {
		total += Q_scnprintf(buffer + total, MAX_STRING_CHARS, "xv 0 %s "
				"yt %d "
				"cstring \"Team %s - %d\" "
				"yt %d "
				"cstring2 \"Name                 Damage     Time Ping\"", entry, y,
				arena->teams[k].name,
				arena->teams[k].damage_dealt,
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

			if (c->resp.score > 0) {
				j = c->resp.score + c->resp.deaths;
			}

			if (level.framenum < 10 * 60 * HZ) {
				sprintf(timebuf, "%d:%02d", sec / 60, sec % 60);
			} else {
				sprintf(timebuf, "%d", sec / 60);
			}

			len = Q_snprintf(entry, sizeof(entry),
					"yt %d cstring \"%-21s %6d     %4s %4d\"", y,
					c->pers.netname,
					c->resp.damage_given,
					timebuf,
					c->ping
			);

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

	}

	// add spectators in fixed order
	total += Q_scnprintf(buffer + total, MAX_STRING_CHARS,
			"yt %d cstring2 \"---- Spectators ----\"", y);
	y += LAYOUT_LINE_HEIGHT;

	for (i = 0; i < arena->spectator_count; i++) {
		ent = arena->spectators[i];
		if (!ent)
			continue;

		if (!ent->client)
			continue;

		if (ent->client->pers.mvdspec)
			continue;

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
	int i, j, k, numranks;
	int y, sec;
	gclient_t *ranks[MAX_CLIENTS];
	gclient_t *c;
	time_t t;
	struct tm *tm;
	edict_t *ent;

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
	}

	// add spectators in fixed order
	total += Q_scnprintf(buffer + total, MAX_STRING_CHARS,
			"yt %d cstring2 \"---- Spectators ----\"", y);
	y += LAYOUT_LINE_HEIGHT;

	for (i = 0; i < arena->spectator_count; i++) {
		ent = arena->spectators[i];
		if (!ent)
			continue;

		if (!ent->client)
			continue;

		if (ent->client->pers.mvdspec)
			continue;

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
	int i, j;
	edict_t *ent;
	arena_team_t *team;

	va_start(argptr, fmt);
	len = Q_vsnprintf(string, sizeof(string), fmt, argptr);
	va_end(argptr);

	if (len == 0) {
		return;
	}

	for (i=0; i<a->team_count; i++) {
		team = &a->teams[i];

		for (j=0; j<MAX_ARENA_TEAM_PLAYERS; j++) {
			ent = team->players[j];

			if (ent && ent->inuse) {
				gi.WriteByte(SVC_CENTERPRINT);
				gi.WriteString(string);
				gi.unicast(ent, true);
			}
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

		G_TeamPart(cl->edict, true);

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

	G_SpectatorsJoin(cl->edict);

	// send all current player skins to this new player
	G_UpdateSkins(cl->edict);

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
	uint8_t i, count;

	count = 0;
	
	for (i=0; i<a->team_count; i++) {
		// just being safe, shouldn't happen
		if (!&a->teams[i])
			continue;

		// all teams need at least 1 player to be considered ready
		if (a->teams[i].player_count == 0)	{
			a->ready = qfalse;
			return;
		}

		// any team not currently ready means arena isn't ready
		if (!a->teams[i].ready) {
			a->ready = qfalse;
			return;
		}

		//
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
 * Match countdowns and clocks. Runs only once per second
 */
void G_CheckTimers(arena_t *a)
{
	// only run once per second
	if (!(a->timer_last_frame + SECS_TO_FRAMES(1) <= level.framenum)) {
		return;
	}
	
	a->timer_last_frame = level.framenum;
	
	uint32_t remaining;
	
	G_UpdateConfigStrings(a);
	
	if (a->state == ARENA_STATE_COUNTDOWN) {
		remaining = (a->round_start_frame - level.framenum) * FRAMETIME;

		if (remaining > 0 && remaining <= 10 && remaining < ((int) g_round_countdown->value - 2)) {
			G_ArenaSound(a, level.sounds.countdown[remaining]);
		}
	}

	// timeout countdown handled in G_TimeoutFrame()
}

void G_UpdateConfigStrings(arena_t *a)
{
	char countdown[10];
	char roundtime[10];
	char timeouttime[10];
	char buf[0xff];

	buf[0] = 0;

	//G_SecsToString(roundtime, (a->round_frame - a->round_start_frame) * FRAMETIME);
	G_SecsToString(roundtime, FRAMES_TO_SECS(a->round_end_frame - a->round_frame));

	switch (a->state) {
	case ARENA_STATE_COUNTDOWN:
		G_SecsToString(countdown, (a->round_start_frame - level.framenum) * FRAMETIME);
		strcat(buf, va("Starting in %s", countdown));
		break;

	case ARENA_STATE_PLAY:
		strcat(buf, va("Playing %s", roundtime));
		break;

	case ARENA_STATE_TIMEOUT:
		G_SecsToString(timeouttime, (a->timein_frame - level.framenum) * FRAMETIME);
		strcat(buf, va("Timeout %s   (%s)", timeouttime, roundtime));
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
void G_UpdateSkins(edict_t *ent)
{
	uint8_t i, j;

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

	team->ready = ready;
}

void G_ForceDemo(arena_t *arena) {
	uint8_t i, j;
	edict_t *ent;
	arena_team_t *team;
	
	if (!g_demo->value) {
		return;
	}
	
	if (arena->recording) {

		for (i=0; i<arena->team_count; i++) {
			team = &arena->teams[i];
			for (j=0; j<MAX_ARENA_TEAM_PLAYERS; j++) {
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
			for (j=0; j<MAX_ARENA_TEAM_PLAYERS; j++) {
				ent = team->players[j];
				if (ent && ent->inuse) {
					G_StuffText(ent, va("record \"%s\"\n", DemoName(ent)));
				}
			}
		}
		
		arena->recording = qtrue;
	}
}

void G_ForceScreenshot(arena_t *arena) {
	uint8_t i, j;
	edict_t *ent;
	arena_team_t *team;

	if (!g_screenshot->value) {
		return;
	}
	
	for (i=0; i<arena->team_count; i++) {
		team = &arena->teams[i];
		for (j=0; j<MAX_ARENA_TEAM_PLAYERS; j++) {
			ent = team->players[j];
			if (ent && ent->inuse) {
				G_StuffText(ent, "wait; screenshot\n");
			}
		}
	}
}

/**
 * Reset after match is finished
 *
 */
void G_EndMatch(arena_t *a, arena_team_t *winner)
{
	uint8_t i;
	char roundtime[10];

	if (winner) {
		for (i = 0; i < MAX_ARENA_TEAM_PLAYERS; i++) {
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

	G_SecsToString(roundtime, a->timelimit);
	G_ConfigString(a, CS_MATCH_STATUS, va("Warmup %s", roundtime));
	G_ConfigString(a, CS_ROUND, G_RoundToString(a));

	for (i = 0; i < a->team_count; i++) {
		G_ForceReady(&a->teams[i], qfalse);
	}
	
	BeginIntermission(a);
	
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
	a->teams_alive = a->team_count;

	G_ConfigString(a, CS_ROUND, G_RoundToString(a));
	G_HideScores(a);
	G_RespawnPlayers(a);
}


/**
 * Lock players in place during timeouts
 *
 */
void G_FreezePlayers(arena_t *a, qboolean freeze) {
	uint8_t i, j;
	edict_t *ent;
	arena_team_t *team;

	if (!a)
		return;

	pmtype_t type;

	if (freeze) {
		type = PM_FREEZE;
	} else {
		type = PM_NORMAL;
	}

	for (i=0; i<a->team_count; i++) {
		team = &a->teams[i];
		for (j=0; j<MAX_ARENA_TEAM_PLAYERS; j++) {
			ent = team->players[j];
			if (ent && ent->inuse) {
				ent->client->ps.pmove.pm_type = type;
			}
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
	ent->client->inventory[ITEM_GRENADES] = 0;	// item & weapon
	ent->client->inventory[ITEM_SLUGS] = 0;
	ent->client->inventory[ITEM_ROCKETS] = 0;
	ent->client->inventory[ITEM_CELLS] = 0;
	ent->client->inventory[ITEM_GRENADES] = 0;
	ent->client->inventory[ITEM_BULLETS] = 0;
	ent->client->inventory[ITEM_SHELLS] = 0;


	flags = a->weapon_flags;

	if (flags < 2)
		flags = ARENAWEAPON_ALL;

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

	// armor
	ent->client->inventory[ITEM_ARMOR_BODY] = a->armor;
	
	// health
	ent->health = a->health;
}


/**
 * Add player to a team
 *
 */
void G_TeamJoin(edict_t *ent, arena_team_type_t type, qboolean forced) {

	if (!ent->client)
		return;

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
		if (!team->players[i]) {	// free player slot, take it
			team->players[i] = ent;
			break;
		}
	}

	// remove them from the spectator list
	G_SpectatorsPart(ent);

	if (!team->captain) {
		team->captain = ent;
	}

	G_bprintf(arena, PRINT_HIGH, "%s joined team %s\n", NAME(ent), team->name);

	// force the skin
	G_SetSkin(ent, team->skin);

	//G_SendStatusBar(ent);

	// throw them into the game
	spectator_respawn(ent, CONN_SPAWNED);
}

/**
 * Remove this player from whatever team they're on
 *
 */
void G_TeamPart(edict_t *ent, qboolean silent) {

	arena_team_t *oldteam;
	int i;

	if (!ent->client)
		return;

	oldteam = TEAM(ent);

	if (!oldteam)
		return;

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
	ent->client->inventory[ITEM_SLUGS] = a->ammo[ITEM_SLUGS];
	ent->client->inventory[ITEM_ROCKETS] = a->ammo[ITEM_ROCKETS];
	ent->client->inventory[ITEM_CELLS] = a->ammo[ITEM_CELLS];
	ent->client->inventory[ITEM_GRENADES] = a->ammo[ITEM_GRENADES];
	ent->client->inventory[ITEM_BULLETS] = a->ammo[ITEM_BULLETS];
	ent->client->inventory[ITEM_SHELLS] = a->ammo[ITEM_SHELLS];

	// armor
	ent->client->inventory[ITEM_ARMOR_BODY] = a->armor;

	// health
	ent->health = a->health;
}

/**
 * refill all inventory for all players in this arena
 */
void G_RefillPlayers(arena_t *a) {
	uint8_t i, j;
	edict_t *ent;

	// for each team
	for (i = 0; i < a->team_count; i++) {

		// for each player
		for (j = 0; j < MAX_ARENA_TEAM_PLAYERS; j++) {
			ent = a->teams[i].players[j];

			if (ent && ent->inuse) {
				G_GiveItems(ent);
			}
		}
	}
}

/**
 * Respawn all players resetting their inventory
 *
 */
void G_RespawnPlayers(arena_t *a) {

	uint8_t i, j;
	edict_t *ent;

	// for each team
	for (i = 0; i < a->team_count; i++) {
		
		// for each player
		for (j = 0; j < MAX_ARENA_TEAM_PLAYERS; j++) {
			ent = a->teams[i].players[j];

			if (ent && ent->inuse) {
				ent->client->resp.damage_given = 0;
				ent->client->resp.damage_recvd = 0;
				ent->killer = NULL;
				G_RefillInventory(ent);
				spectator_respawn(ent, CONN_SPAWNED);
			}

			a->teams[i].players_alive = a->teams[i].player_count;
		}
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
void G_SetSkin(edict_t *ent, const char *skin)
{
	if (!ent->client) {
		return;
	}

	strcpy(ent->client->pers.skin, skin);

	// let everyone know this player's new skin
	G_ConfigString(
		ARENA(ent),
		CS_PLAYERSKINS + (ent - g_edicts) - 1,
		va("%s\\%s",
			NAME(ent),
			skin
		)
	);
}


/**
 * Display scores layout to all arena players
 *
 */
void G_ShowScores(arena_t *a) {
	uint8_t i, j;
	edict_t *ent;

	// for each team
	for (i = 0; i < a->team_count; i++) {

		// for each player
		for (j = 0; j < MAX_ARENA_TEAM_PLAYERS; j++) {
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
 *
 */
void G_HideScores(arena_t *a) {
	uint8_t i, j;
	edict_t *ent;

	// for each team
	for (i = 0; i < a->team_count; i++) {

		// for each player
		for (j = 0; j < MAX_ARENA_TEAM_PLAYERS; j++) {
			ent = a->teams[i].players[j];

			if (ent && ent->inuse) {
				ent->client->layout = 0;
			}
		}
	}
}


/**
 * Begin a round
 *
 */
void G_StartRound(arena_t *a) {
	uint8_t i;

	for (i=0; i<a->team_count; i++) {
		a->teams[i].players_alive = a->teams[i].player_count;
	}

	a->state = ARENA_STATE_PLAY;
	a->round_start_frame = a->round_frame - SECS_TO_FRAMES(1);
	a->round_end_frame = a->round_start_frame + SECS_TO_FRAMES(a->timelimit);

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
		G_ArenaSound(a, level.sounds.timein);
		return;
	}

	// countdown timer
	int framesleft = a->timein_frame - level.framenum;
	if (framesleft > 0 && framesleft % SECS_TO_FRAMES(1) == 0) {

		G_UpdateConfigStrings(a);

		uint32_t remaining;
		remaining = (a->timein_frame - level.framenum) * FRAMETIME;

		if (remaining > 0 && remaining <= 10) {
			G_ArenaSound(a, level.sounds.countdown[remaining]);
		}
	}

	a->match_frame++;
}


/**
 * Get time string (mm:ss) from seconds
 *
 */
void G_SecsToString(char *out, int seconds)
{
	int mins;

	mins = seconds / 60;
	seconds -= (mins * 60);

	sprintf(out, "%d:%.2d", mins, seconds);
}

// 
arena_team_t *G_GetWinningTeam(arena_t *a) {
	uint8_t i;

	static uint8_t alivecount = 0;
	static arena_team_t *winner;

	if (!a)
		return NULL;

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
		a->ammo[ITEM_SLUGS] = (int) g_ammo_slugs->value;
		a->ammo[ITEM_ROCKETS] = (int) g_ammo_rockets->value;
		a->ammo[ITEM_CELLS] = (int) g_ammo_cells->value;
		a->ammo[ITEM_GRENADES] = (int) g_ammo_grenades->value;
		a->ammo[ITEM_BULLETS] = (int) g_ammo_bullets->value;
		a->ammo[ITEM_SHELLS] = (int) g_ammo_shells->value;
		a->team_count = (int) g_team_count->value;
		a->timelimit = (int) g_round_timelimit->value;
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

	memcpy(a->infinite, m->infinite, sizeof(a->infinite));

	// save defaults for voting
	memcpy(a->defaultammo, a->ammo, sizeof(a->defaultammo));
	memcpy(a->defaultinfinite, a->infinite, sizeof(a->defaultinfinite));
	a->original_damage_flags = a->damage_flags;
	a->original_weapon_flags = a->weapon_flags;
}

qboolean G_Teammates(edict_t *p1, edict_t *p2) {
	if (!(p1->client && p2->client)) {
		return qfalse;
	}
	return TEAM(p1) == TEAM(p2);
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

void G_ResetTeam(arena_team_t *t) {
	uint8_t i;
	edict_t *player;
	
	t->damage_dealt = 0;
	t->damage_taken = 0;
	
	t->ready = qfalse;

	// respawn all players
	for (i = 0; i < MAX_ARENA_TEAM_PLAYERS; i++) {
		if (t->players[i]) {
			player = t->players[i];
			
			memset(&player->client->resp, 0, sizeof(player->client->resp));
			memset(&player->client->level.vote, 0, sizeof(player->client->level.vote));
			
			player->movetype = MOVETYPE_NOCLIP; // don't leave a body
			player->client->pers.ready = qfalse;
			
			if (g_team_reset->value) {
				G_TeamPart(player, true);
			} else {
				respawn(player);
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
 * Use glib's regex stuff to test patterns. Just returns yey or ney
 * if the pattern matches, no way to get the actual part that matches
 */
qboolean G_RegexMatch(const char *pattern, const char *string) {

	GRegex *regex;
	GMatchInfo *matchinfo;
	qboolean matches;

	regex = g_regex_new(pattern, 0, 0, 0);
	g_regex_match(regex, string, 0, &matchinfo);

	if (g_match_info_matches(matchinfo)) {
		matches = qtrue;
	} else {
		matches = qfalse;
	}

	g_match_info_free(matchinfo);
	g_regex_unref(regex);

	return matches;
}

/**
 * Drop every player from a team
 */
void G_RemoveAllTeamPlayers(arena_team_t *team, qboolean silent) {
	uint8_t i;
	for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
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
	
	for (i=0; i<a->team_count; i++) {
		G_ResetTeam(&a->teams[i]);
	}

	G_FinishArenaVote(a);
}

void G_CheckIntermission(arena_t *a) {
	int32_t i;
	edict_t *ent;
	
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
			
			for (i = 0, ent = &g_edicts[1]; i < game.maxclients; i++, ent++) {
				if (ent->client->pers.connected > CONN_CONNECTED) {
					G_ArenaScoreboardMessage(ent, qtrue);
				}
			}	
		} else if (delta == exit_delta) {
			G_ResetArena(a);
		}
	}
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

	for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
		ent = t->players[i];
		if (ent && ent->inuse) {
			gi.unicast(ent, reliable);
		}
	}
}

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

void G_RandomizeAmmo(uint16_t *out) {
	out[ITEM_SHELLS] = genrand_int32() & 0xF;
	out[ITEM_BULLETS] = genrand_int32() & 0xFF;
	out[ITEM_GRENADES] = genrand_int32() & 0xF;
	out[ITEM_CELLS] = genrand_int32() & 0xFF;
	out[ITEM_ROCKETS] = genrand_int32() & 0xF;
	out[ITEM_SLUGS] = genrand_int32() & 0xF;
}

/**
 * Generate a player-specific statusbar
 *
 */
const char *G_CreatePlayerStatusBar(edict_t *player)
{
	static char	*statusbar;
	static char weaponhud[175];		// the weapon icons
	static char ammohud[135];		// the ammo counts
	int			hud_x, hud_y;

	if (!player->client)
		return "";

	hud_y = 0;
	hud_x = -25;

	weaponhud[0] = 0;
	ammohud[0] = 0;

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
			"pic 2 "
		"endif "

		// armor
		"if 4 "
			"xv 200 "
			"rnum "
			"xv 250 "
			"pic 4 "
		"endif "

		"yb -50 "

		// picked up item
		"if 7 "
			"xv 0 "
			"pic 7 "
			"xv 26 "
			"yb -42 "
			"stat_string 8 "
			"yb -50 "
		"endif "

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

		// status
		"if 31 "
			"yb -60 "
			"xv 0 "
			"stat_string 31 "
		"endif "

		// round
		"if 28 "
			"xr -24 "
			"yt 30 "
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
 *
 */
const char *G_CreateSpectatorStatusBar(edict_t *player)
{
	static char	*statusbar;

	if (!player->client)
		return "";

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
			"pic 2 "
		"endif "

		// armor
		"if 4 "
			"xv 200 "
			"rnum "
			"xv 250 "
			"pic 4 "
		"endif "

		"yb -50 "

		// picked up item
		"if 7 "
			"xv 0 "
			"pic 7 "
			"xv 26 "
			"yb -42 "
			"stat_string 8 "
			"yb -50 "
		"endif "

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

		// status
		"if 31 "
			"yb -60 "
			"xv 0 "
			"stat_string 31 "
		"endif "

		// round
		"if 28 "
			"xr -24 "
			"yt 30 "
			"stat_string 28 "
		"endif "
	);

	return statusbar;
}

/**
 * Send player/spec their specific statusbar
 */
void G_SendStatusBar(edict_t *ent)
{
	gi.WriteByte(SVC_CONFIGSTRING);
	gi.WriteShort(CS_STATUSBAR);

	if (TEAM(ent)) {
		gi.WriteString(G_CreatePlayerStatusBar(ent));
	} else {
		gi.WriteString(G_CreateSpectatorStatusBar(ent));
	}

	gi.unicast(ent, true);
}


/**
 * Update statusbars for all arena players
 *
 */
void G_UpdatePlayerStatusBars(arena_t *a)
{
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
void G_BeginRoundIntermission(arena_t *a)
{
	// we're already in intermission
	if (a->state == ARENA_STATE_ROUNDPAUSE) {
		return;
	}

	a->state = ARENA_STATE_ROUNDPAUSE;
	a->round_end_frame = level.framenum + SECS_TO_FRAMES((int) g_round_end_time->value);
	a->round_intermission_start = level.framenum;
	a->round_intermission_end = a->round_intermission_start + SECS_TO_FRAMES(5);

	G_ShowScores(a);
}

/**
 * Round intermission is over, reset and proceed to next round
 */
void G_EndRoundIntermission(arena_t *a)
{
	if (a->state != ARENA_STATE_ROUNDPAUSE) {
		return;
	}

	a->round_intermission_start = 0;
	a->round_intermission_end = 0;

	G_EndRound(a, NULL);
}

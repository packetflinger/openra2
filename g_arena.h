/*
Copyright (C) 2016 Packetflinger.com

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
#ifndef ARENA_H
#define ARENA_H

#define MAX_INVENTORY			33

#define MAX_ARENAS				9
#define MAX_TEAMS				5
#define MAX_ARENA_TEAM_PLAYERS	10
#define MAX_TEAM_NAME			20
#define MAX_TEAM_SKIN			25
#define MAX_ROUNDS				21

#define str_equal(x, y)			(Q_stricmp((x), (y)) == 0)

#define SECS_TO_FRAMES(seconds)	(int)((seconds) * HZ)
#define FRAMES_TO_SECS(frames)	(int)((frames) * FRAMETIME)

#define LAYOUT_LINE_HEIGHT		8
#define LAYOUT_CHAR_WIDTH		8

#define NAME(e) (e->client->pers.netname)
#define TEAM(e) (e->client->pers.team)
#define ARENA(e) (e->client->pers.arena)

#define ROUNDOVER(a) (a->state == ARENA_STATE_PLAY && a->teams_alive == 1)

#define WEAPONFLAG_MASK			0x7FF
#define FOR_EACH_ARENA(a) \
    LIST_FOR_EACH(arena_t, a, &g_arenalist, entry)

extern list_t	g_arenalist;

#ifdef _WIN32
#define DATE_FORMAT ("%b %d, %Y %H:%M ")
#else
#define DATE_FORMAT ("%b %e, %Y %H:%M ")
#endif

/**
 * Used for parsing weapons flags (and associated data) for initial map spawn
 * and voting.
 */
typedef struct {
	uint32_t weaponflags;
	uint16_t ammo[MAX_INVENTORY];
	qboolean infinite[MAX_INVENTORY];
} temp_weaponflags_t;

typedef enum {
	WINNER_NONE,
	WINNER_HOME,
	WINNER_AWAY,
	WINNER_TIE,
} round_winner_t;


typedef enum {
	TEAM_SPECTATORS,
	TEAM_RED,
	TEAM_BLUE,
	TEAM_GREEN,
	TEAM_YELLOW,
	TEAM_AQUA
} arena_team_type_t;


typedef struct {
	int		num;
	char	name[20];
} pmenu_arena_t;

typedef struct {
	int32_t		proposal;						// which VOTE_*
	int8_t		index;							// matches index in client_level_t
	uint32_t	framenum;						// expiration (level.framenum + vote len cvar)
	uint32_t	value;							//
	edict_t		*victim;						// target for kick/mute
	edict_t		*initiator;						// who called the vote
	char		map[MAX_QPATH];					// the map
	uint16_t	items[MAX_INVENTORY];			// ammo
	qboolean	infinite[MAX_INVENTORY];		// inf ammo
	char		original[MAX_STRING_CHARS];		// the original vote command
} arena_vote_t;

// don't change the order of these
typedef enum {
	ARENA_STATE_WARMUP,
	ARENA_STATE_COUNTDOWN,
	ARENA_STATE_PLAY,
	ARENA_STATE_OVERTIME,
	ARENA_STATE_TIMEOUT,
	ARENA_STATE_ROUNDPAUSE,		// the gap between rounds in a match
	ARENA_STATE_INTERMISSION,
} arena_state_t;


// the arena to join on connect by default
typedef enum {
	ARENA_DEFAULT_FIRST,		// always #1
	ARENA_DEFAULT_POPULAR,	// which ever has the most players
	ARENA_DEFAULT_RANDOM,		// random one
} arena_default_t;





typedef struct {
	char 				name[MAX_TEAM_NAME];
	char				skin[MAX_TEAM_SKIN];
	edict_t				*players[MAX_ARENA_TEAM_PLAYERS];
	edict_t				*captain;
	arena_team_type_t 	type;
	int8_t				player_count;
	int8_t				players_alive;
	qboolean			locked;
	qboolean			ready;
	uint32_t			damage_dealt;
	uint32_t			damage_taken;
	list_t				entry;
} arena_team_t;


typedef struct {
	uint8_t         number;                      // level.arenas[] index
	char            name[MAX_TEAM_NAME];         // name used in menu
	arena_state_t   state;
	uint32_t        match_frame;                 // total frames this match
	uint32_t        round_frame;                 // current frame this round
	uint32_t        round_start_frame;           // when this round started
	uint32_t        round_end_frame;             // when this round ended
	uint32_t        timeout_frame;               // when timeout was called
	uint32_t        timein_frame;                // when timeout will end
	edict_t         *timeout_caller;             // player who called it
	int             countdown;
	uint8_t         round_limit;                 // how many rounds in a match
	uint8_t         current_round;
	uint32_t        weapon_flags;
	uint32_t        damage_flags;
	uint32_t        original_weapon_flags;       // for resetting
	uint32_t        original_damage_flags;       // for resetting
	uint16_t        health;
	uint16_t        armor;
	char            oldscores[MAX_STRING_CHARS];
	edict_t         *clients[MAX_CLIENTS];       // all players and specs
	int             client_count;
	edict_t         *spectators[MAX_CLIENTS];    // make this not suck later
	uint8_t         spectator_count;
	arena_team_t    teams[MAX_TEAMS];            // [team_count]
	uint8_t         team_count;                  // how many teams are enabled
	uint8_t         player_count;
	uint16_t        ammo[MAX_INVENTORY];
	uint16_t        defaultammo[MAX_INVENTORY];
	qboolean        infinite[MAX_INVENTORY];
	qboolean        defaultinfinite[MAX_INVENTORY];
	int32_t         ready_think_frame;           // next time we check if everyone is ready
	int32_t         ready_notify_frame;          // next time we nag about being ready
	qboolean        ready;                       // are all team players ready?
	int32_t         timer_last_frame;            // the frame we last ran timers
	qboolean        recording;                   // were players forced to start recording demos?
	uint32_t        intermission_framenum;       // time the intermission was started
	uint32_t        intermission_exit;           // time the intermission was exited
	vec3_t          intermission_origin;         // intermission spot
	vec3_t          intermission_angle;          // view angle from ^ spot
	uint32_t        version;                     // map version
	list_t          entry;                       // for making linked list of arenas
	arena_vote_t    vote;                        // the current local vote
	qboolean        modified;                    // defaults have been changed via votes
	uint32_t        timelimit;                   // how long to allow each round
	int32_t         round_intermission_start;    // frame round intermission started
	int32_t         round_intermission_end;      // frame we should end round intermission
	int32_t         teams_alive;                 // how many teams have players alive
	uint32_t        countdown_start_frame;       // frame number when the count started
} arena_t;



// maps contain multiple arenas
typedef struct {
	char        name[50];
	uint8_t     arena;
	uint8_t     teams;
	uint32_t    weapon_flags;
	uint32_t    damage_flags;
	uint8_t     rounds;
	uint32_t    round_timelimit;
	uint32_t    health;
	uint32_t    armor;
	uint32_t    timelimit;
	// ammo
	uint32_t    slugs;
	uint32_t    rockets;
	uint32_t    cells;
	uint32_t    grenades;
	uint32_t    bullets;
	uint32_t    shells;
	uint16_t    ammo[MAX_INVENTORY];
	qboolean    infinite[MAX_INVENTORY];
} arena_entry_t;



void change_arena(edict_t *self);
const char *DemoName(edict_t *ent);
void G_ArenaCast(arena_t *a, qboolean reliable);
void G_ArenaPlayerboardMessage(edict_t *ent, qboolean reliable);
void G_ArenaScoreboardMessage(edict_t *ent, qboolean reliable);
void G_ArenaSound(arena_t *a, int index);
void G_ArenaStuff(arena_t *a, const char *command);
void G_ArenaThink(arena_t *a);
void G_AsciiToConsole(char *out, char *in);
void G_bprintf(arena_t *arena, int level, const char *fmt, ...);
void G_BuildMenu(void);
size_t G_BuildPlayerboard(char *buffer, arena_t *arena);
size_t G_BuildPregameScoreboard(char *buffer, gclient_t *client, arena_t *arena);
size_t G_BuildScoreboard(char *buffer, gclient_t *client, arena_t *arena);
size_t G_BuildScoreboard_V(char *buffer, gclient_t *client, arena_t *arena);
int G_CalcArenaRanks(gclient_t **ranks, arena_team_t *team);
void G_Centerprintf(arena_t *a, const char *fmt, ...);
void G_CheckArenaRules(arena_t *a);
qboolean G_CheckArenaVote(arena_t *a);	// in g_vote.c
void G_CheckIntermission(arena_t *a);
void G_CheckReady(arena_t *a);
void G_CheckTimers(arena_t *a);
void G_ChangeArena(gclient_t *cl, arena_t *arena);
void G_ClearRoundInfo(arena_t *a);
void G_ConfigString(arena_t *arena, uint16_t index, const char *string);
const char *G_CreatePlayerStatusBar(edict_t *player);
char *G_DamageFlagsToString(uint32_t df);
void G_EndMatch(arena_t *a, arena_team_t *winner);
void G_EndRound(arena_t *a, arena_team_t *winner);
void G_FinishArenaVote(arena_t *a);
void G_ForceDemo(arena_t *arena);
void G_ForceReady(arena_team_t *team, qboolean ready);
void G_FreezePlayers(arena_t *a, qboolean freeze);
arena_team_t *G_GetWinningTeam(arena_t *a);
void G_GiveItems(edict_t *ent);
void G_HideScores(arena_t *a);
void G_InitArenaTeams(arena_t *arena);	// in g_spawn.c
void G_TeamJoin(edict_t *ent, arena_team_type_t type, qboolean forced);
void G_MergeArenaSettings(arena_t *a, arena_entry_t *m);
size_t G_ParseMapSettings(arena_entry_t *entry, const char *mapname);
int G_PlayerCmp(const void *p1, const void *p2);
void G_TeamPart(edict_t *ent, qboolean silent);
void G_RandomizeAmmo(uint16_t *out);
void G_RefillInventory(edict_t *ent);
void G_RefillPlayers(arena_t *a);
void G_RecreateArena(arena_t *a);
qboolean G_RegexMatch(const char *pattern, const char *string);
void G_RemoveAllTeamPlayers(arena_team_t *team, qboolean silent);
void G_ResetArena(arena_t *a);
void G_ResetTeam(arena_team_t *t);
void G_RespawnPlayers(arena_t *a);
char *G_RoundToString(arena_t *a);
void G_SecsToString(char *out, int seconds);
void G_SelectBestWeapon(edict_t *ent);
void G_SendStatusBar(edict_t *ent);
void G_SetSkin(edict_t *ent, const char *skin);
void G_ShowScores(arena_t *a);
void G_SpectatorsJoin(edict_t *ent);
void G_SpectatorsPart(edict_t *ent);
void G_StartingWeapon(edict_t *ent);
void G_StartRound(arena_t *a);
void G_TeamCast(arena_team_t *t, qboolean reliable);
qboolean G_Teammates(edict_t *p1, edict_t *p2);
void G_TimeoutFrame(arena_t *a);
void G_UpdateArenaVote(arena_t *a);
void G_UpdateConfigStrings(arena_t *a);
void G_UpdatePlayerStatusBars(arena_t *a);
void G_UpdateSkins(edict_t *ent);
char *G_WeaponFlagsToString(arena_t *a);
void G_BeginRoundIntermission(arena_t *a);
void G_EndRoundIntermission(arena_t *a);

#endif // ARENA_H

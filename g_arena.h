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
#define MAX_ARENAS				9
#define MAX_ARENA_TEAM_PLAYERS	10
#define MAX_TEAM_NAME			20
#define MAX_TEAM_SKIN			25

#define str_equal(x, y)			(Q_stricmp((x), (y)) == 0)

#define SECS_TO_FRAMES(seconds)	(int)((seconds) * HZ)
#define FRAMES_TO_SECS(frames)	(int)((frames) * FRAMETIME)

#define LAYOUT_LINE_HEIGHT		8
#define LAYOUT_CHAR_WIDTH		8

#define NAME(e) (e->client->pers.netname)
#define TEAM(e) (e->client->pers.team)
#define ARENA(e) (e->client->pers.arena)

#define FOR_EACH_ARENA(a) \
    LIST_FOR_EACH(arena_t, a, &g_arenalist, entry)
	
extern list_t	g_arenalist;

#ifdef _WIN32
#define DATE_FORMAT ("%b %d, %Y %H:%M ")
#else
#define DATE_FORMAT ("%b %e, %Y %H:%M ")
#endif

typedef enum {
	WINNER_NONE,
	WINNER_HOME,
	WINNER_AWAY,
	WINNER_TIE,
} round_winner_t;


typedef enum {
	ARENA_TEAM_SPEC,
	ARENA_TEAM_HOME,
	ARENA_TEAM_AWAY,
} arena_team_type_t;


typedef struct {
	int		num;
	char	name[20];
} pmenu_arena_t;


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
	uint32_t			damage_dealt;
	uint32_t			damage_taken;
} arena_team_t;



typedef struct {
	int				number;
	char			name[MAX_TEAM_NAME];
	arena_state_t	state;
	int				countdown;
	int				player_count;
	int				spectator_count;
	int				round_limit;
	int				current_round;
	int				weapon_flags;
	int				damage_flags;
	int				health;
	int				armor;
	int				round_start_frame;
	int				round_end_frame;
	int				timeout_frame;
	int				timein_frame;
	char        	oldscores[MAX_STRING_CHARS];
	edict_t			*timeout_caller;
	edict_t			*clients[MAX_CLIENTS];
	int				client_count;
	arena_team_t	team_home;
	arena_team_t	team_away;
	int				slugs;
	int				rockets;
	int				cells;
	int				grenades;
	int				bullets;
	int				shells;
	int32_t			ready_think_frame;
	int32_t			ready_notify_frame;
	qboolean		ready;
	int32_t			timer_last_frame;		// the frame we last ran timers
	qboolean		recording;				// were players forced to start recording demos?
	int         	intermission_framenum;	// time the intermission was started
    int         	intermission_exit;		// time the intermission was exited
    vec3_t      	intermission_origin;
    vec3_t      	intermission_angle;
	int				version;				// map version
	list_t			entry;					// for making linked list of arenas
} arena_t;



// maps contain multiple arenas
typedef struct {
	char		name[50];
	uint8_t		arena;
	uint32_t	weapon_flags;
	uint32_t	damage_flags;
	uint8_t 	rounds;
	uint32_t 	round_timelimit;
	uint32_t	health;
	uint32_t	armor;
	// ammo
	uint32_t	slugs;
	uint32_t	rockets;
	uint32_t	cells;
	uint32_t	grenades;
	uint32_t	bullets;
	uint32_t	shells;
} arena_entry_t;



void change_arena(edict_t *self);
void G_ArenaScoreboardMessage(edict_t *ent, qboolean reliable);
void G_ArenaPlayerboardMessage(edict_t *ent, qboolean reliable);
void G_ArenaSound(arena_t *a, int index);
void G_ArenaStuff(arena_t *a, const char *command);
void G_ArenaThink(arena_t *a);
void G_bprintf(arena_t *arena, int level, const char *fmt, ...);
void G_BuildMenu(void);
size_t G_BuildScoreboard(char *buffer, gclient_t *client, arena_t *arena);
size_t G_BuildPregameScoreboard(char *buffer, gclient_t *client, arena_t *arena);
size_t G_BuildScoreboard_V(char *buffer, gclient_t *client, arena_t *arena);
size_t G_BuildPlayerboard(char *buffer, arena_t *arena);
int G_CalcArenaRanks(gclient_t **ranks, arena_team_t *team);
void G_Centerprintf(arena_t *a, const char *fmt, ...);
void G_ChangeArena(gclient_t *cl, arena_t *arena);
void G_CheckReady(arena_t *a);
void G_CheckTimers(arena_t *a);
void G_EndMatch(arena_t *a, arena_team_t *winner);
void G_EndRound(arena_t *a, arena_team_t *winner);
void G_ForceReady(arena_team_t *team, qboolean ready);
void G_FreezePlayers(arena_t *a, qboolean freeze);
void G_GiveItems(edict_t *ent);
void G_HideScores(arena_t *a);
void G_JoinTeam(edict_t *ent, arena_team_type_t type, qboolean forced);
void G_PartTeam(edict_t *ent, qboolean silent);
int G_PlayerCmp(const void *p1, const void *p2);
void G_RefillInventory(edict_t *ent);
void G_RespawnPlayers(arena_t *a);
char *G_RoundToString(arena_t *a);
void G_SetSkin(edict_t *ent, const char *skin);
void G_ShowScores(arena_t *a);
void G_StartRound(arena_t *a);
void G_StartingWeapon(edict_t *ent);
void G_TimeoutFrame(arena_t *a);
const char *G_SecsToString (int seconds);
arena_team_t *G_GetWinningTeam(arena_t *a);
void G_CheckArenaRules(arena_t *a);
void G_MergeArenaSettings(arena_t *a, arena_entry_t *m);
qboolean G_Teammates(edict_t *p1, edict_t *p2);
size_t G_ParseMapSettings(arena_entry_t *entry, const char *mapname);
void G_SelectBestWeapon(edict_t *ent);
void G_AsciiToConsole(char *out, char *in);
const char *DemoName(edict_t *ent);
void G_ForceDemo(arena_t *arena);
void G_ResetArena(arena_t *a);
void G_CheckIntermission(arena_t *a);
void G_ResetTeam(arena_team_t *t);
void G_ClearRoundInfo(arena_t *a);
void G_UpdateConfigStrings(arena_t *a);

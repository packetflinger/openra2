/*
Copyright (C) 2016-2021 Packetflinger.com

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

#ifndef ARENA_H
#define ARENA_H

#define MAX_INVENTORY           33
#define MAX_ARENAS              9
#define MAX_ROUNDS              21
#define MAX_TEAMS               5
#define MAX_ARENA_TEAM_PLAYERS  10
#define MAX_TEAM_NAME           20
#define MAX_TEAM_SKIN           25

#define str_equal(x, y)          (Q_stricmp((x), (y)) == 0)

#define SECS_TO_FRAMES(seconds)  (int)((seconds) * HZ)
#define FRAMES_TO_SECS(frames)   (int)((frames) * FRAMETIME)

#define LAYOUT_LINE_HEIGHT       8
#define LAYOUT_CHAR_WIDTH        8

#define NAME(e)                 (e->client->pers.netname)
#define TEAM(e)                 (e->client->pers.team)
#define ARENA(e)                (e->client->pers.arena)
#define ARENASTATE(e)           (e->client->pers.arena->state)

#define IS_PLAYER(e)            (TEAM(e) && (TEAM(e)->type != TEAM_SPECTATORS))
#define IS_SPECTATOR(e)         (TEAM(e) && (TEAM(e)->type == TEAM_SPECTATORS))

#define ROUNDOVER(a) (a->state == ARENA_STATE_PLAY && a->teams_alive == 1)

#define WEAPONFLAG_MASK          0x7FF
#define FOR_EACH_ARENA(a) \
    LIST_FOR_EACH(arena_t, a, &g_arenalist, entry)

extern list_t    g_arenalist;

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

typedef struct {
    int  num;
    char name[20];
} pmenu_arena_t;

typedef struct {
    int32_t     proposal;                    // which VOTE_*
    int8_t      index;                       // matches index in client_level_t
    uint32_t    framenum;                    // expiration (level.framenum + vote len cvar)
    uint32_t    value;                       //
    edict_t     *victim;                     // target for kick/mute
    edict_t     *initiator;                  // who called the vote
    char        map[MAX_QPATH];              // the map
    uint16_t    items[MAX_INVENTORY];        // ammo
    qboolean    infinite[MAX_INVENTORY];     // inf ammo
    char        original[MAX_STRING_CHARS];  // the original vote command
} arena_vote_t;

// the arena to join on connect by default
typedef enum {
    ARENA_DEFAULT_FIRST,     // always #1
    ARENA_DEFAULT_POPULAR,   // which ever has the most players
    ARENA_DEFAULT_RANDOM,    // random one
} arena_default_t;

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
    qboolean    fastswitch;
    uint32_t    mode;       // normal, redrover, comp, etc
    qboolean    corpseview; // gag, see players chasing
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
qboolean G_Arenamates(edict_t *p1, edict_t *p2);
char *G_ArenaModeString(arena_t *a);
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
qboolean G_CheckArenaVote(arena_t *a);    // in g_vote.c
void G_CheckIntermission(arena_t *a);
void G_CheckReady(arena_t *a);
void G_CheckArenaReady(arena_t *a);
qboolean G_CheckTeamAlive(edict_t *ent);
void G_CheckTeamReady(arena_team_t *t);
void G_CheckTimers(arena_t *a);
void G_ChangeArena(edict_t *ent, arena_t *arena);
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
void G_InitArenaTeams(arena_t *arena);    // in g_spawn.c
qboolean G_IsRoundOver(arena_t *a);
void G_MergeArenaSettings(arena_t *a, arena_entry_t *m);
size_t G_ParseMapSettings(arena_entry_t *entry, const char *mapname);
int G_PlayerCmp(const void *p1, const void *p2);
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
void G_SetESkin(edict_t *target);
void G_SetSkin(edict_t *skinfor);
void G_SetTSkin(edict_t *target);
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
void G_ApplyDefaults(arena_t *a);
void update_playercounts(arena_t *a);

#endif // ARENA_H

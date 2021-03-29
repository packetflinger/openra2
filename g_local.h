/*
Copyright (C) 1997-2001 Id Software, Inc.
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
// g_local.h -- local definitions for game module

#include "q_shared.h"
#include "q_list.h"
#include <glib.h>

#if defined(_WIN32) && !(defined(__MINGW32__) || defined(__MINGW64__))
#include "GitRevisionInfo.h" // Derived from template via GitWCRev
#endif // _WIN32

#define true 		1
#define false		0

#define TAG_GAME    765     // clear when unloading the dll
#define TAG_LEVEL   766     // clear when loading a new level

// define GAME_INCLUDE so that game.h does not define the
// short, server-visible gclient_t and edict_t structures,
// because we define the full size ones in this file
#define GAME_INCLUDE
#include "g_public.h"
#include "g_arena.h"

// should be set at build-time in Makefile
#ifndef OPENRA2_VERSION
#define OPENRA2_VERSION "0000000"
#endif

// should be set at build-time in Makefile
#ifndef OPENRA2_REVISION
#define OPENRA2_REVISION "r00"
#endif

#if USE_FPS
#define G_GMF_VARIABLE_FPS GMF_VARIABLE_FPS
#else
#define G_GMF_VARIABLE_FPS 0
#endif

// features this game supports
#define G_FEATURES  (GMF_CLIENTNUM | GMF_PROPERINUSE | GMF_MVDSPEC | \
                     GMF_WANT_ALL_DISCONNECTS | G_GMF_VARIABLE_FPS)

// the "gameversion" client command will print this plus compile date
#define GAMEVERSION "OpenRA2"

// protocol bytes that can be directly added to messages
#define SVC_MUZZLEFLASH     1
#define SVC_MUZZLEFLASH2    2
#define SVC_TEMP_ENTITY     3
#define SVC_LAYOUT          4
#define SVC_INVENTORY       5
#define SVC_SOUND           9
#define SVC_STUFFTEXT       11
#define SVC_CONFIGSTRING    13
#define SVC_CENTERPRINT     15

//==================================================================
// for setting which weapons are allowed in arenas - all weaps == 2046
#define ARENAWEAPON_SHOTGUN         2
#define ARENAWEAPON_SUPERSHOTGUN    4
#define ARENAWEAPON_MACHINEGUN      8
#define ARENAWEAPON_CHAINGUN        16
#define ARENAWEAPON_GRENADE         32
#define ARENAWEAPON_GRENADELAUNCHER 64
#define ARENAWEAPON_HYPERBLASTER    128
#define ARENAWEAPON_ROCKETLAUNCHER  256
#define ARENAWEAPON_RAILGUN         512
#define ARENAWEAPON_BFG             1024
#define ARENAWEAPON_ALL             2046

// All damage apples by default except when below flags are applied
#define ARENADAMAGE_SELF            1   // no self health damage
#define ARENADAMAGE_SELF_ARMOR      2   // no self armor dmg (requires health protection)
#define ARENADAMAGE_TEAM            4   // no team damage
#define ARENADAMAGE_TEAM_ARMOR      8   // no team armor
#define ARENADAMAGE_FALL            16  // no falling damage
#define ARENADAMAGE_ALL             31

#define NOHURT(ent, d)    (ARENA(ent)->damage_flags & ARENADAMAGE_##d)

// view pitching times
#define DAMAGE_TIME     0.5
#define FALL_TIME       0.3


// edict->spawnflags
// these are set with checkboxes on each entity in the map editor
#define SPAWNFLAG_NOT_EASY          0x00000100
#define SPAWNFLAG_NOT_MEDIUM        0x00000200
#define SPAWNFLAG_NOT_HARD          0x00000400
#define SPAWNFLAG_NOT_DEATHMATCH    0x00000800
#define SPAWNFLAG_NOT_COOP          0x00001000

#define INHIBIT_MASK                (SPAWNFLAG_NOT_EASY | SPAWNFLAG_NOT_MEDIUM | \
                                     SPAWNFLAG_NOT_HARD | SPAWNFLAG_NOT_COOP | \
                                     SPAWNFLAG_NOT_DEATHMATCH)

// edict->flags
#define FL_FLY                  0x00000001
#define FL_SWIM                 0x00000002  // implied immunity to drowining
#define FL_IMMUNE_LASER         0x00000004
#define FL_INWATER              0x00000008
#define FL_GODMODE              0x00000010
#define FL_NOTARGET             0x00000020
#define FL_IMMUNE_SLIME         0x00000040
#define FL_IMMUNE_LAVA          0x00000080
#define FL_PARTIALGROUND        0x00000100  // not all corners are valid
#define FL_WATERJUMP            0x00000200  // player jumping out of water
#define FL_TEAMSLAVE            0x00000400  // not the first on the team
#define FL_NO_KNOCKBACK         0x00000800
#define FL_POWER_ARMOR          0x00001000  // power armor (if any) is active
#define FL_NOCLIP_PROJECTILE    0x00002000  // projectile hack
#define FL_MEGAHEALTH           0x00004000  // for megahealth kills tracking
#define FL_ACCELERATE           0x20000000  // accelerative movement
#define FL_HIDDEN               0x40000000  // used for banned items
#define FL_RESPAWN              0x80000000  // used for item respawning

// variable server FPS
#if USE_FPS
#define HZ              game.framerate
#define FRAMETIME       game.frametime
#define FRAMEDIV        game.framediv
#define FRAMESYNC       !(level.framenum % game.framediv)
#else
#define HZ              BASE_FRAMERATE
#define FRAMETIME       BASE_FRAMETIME_1000
#define FRAMEDIV        1
#define FRAMESYNC       1
#endif

#define KEYFRAME(x)   (level.framenum + (x) - (level.framenum % FRAMEDIV))

#define NEXT_FRAME(ent, func) \
    ((ent)->think = (func), (ent)->nextthink = level.framenum + 1)

#define NEXT_KEYFRAME(ent, func) \
    ((ent)->think = (func), (ent)->nextthink = KEYFRAME(FRAMEDIV))


// memory tags to allow dynamic memory to be cleaned up
#define TAG_GAME    765     // clear when unloading the dll
#define TAG_LEVEL   766     // clear when loading a new level


#define MELEE_DISTANCE  80

#define BODY_QUEUE_SIZE     8

#define MAX_NETNAME     16
#define MAX_SKINNAME    24

typedef enum {
    DAMAGE_NO,
    DAMAGE_YES,         // will take damage if hit
    DAMAGE_AIM          // auto targeting recognizes this
} damage_t;

typedef enum {
    WEAPON_READY,
    WEAPON_ACTIVATING,
    WEAPON_DROPPING,
    WEAPON_FIRING
} weaponstate_t;

typedef enum {
    AMMO_BULLETS,
    AMMO_SHELLS,
    AMMO_ROCKETS,
    AMMO_GRENADES,
    AMMO_CELLS,
    AMMO_SLUGS
} ammo_t;

typedef enum {
    ITEM_NULL,

    ITEM_ARMOR_BODY,
    ITEM_ARMOR_COMBAT,
    ITEM_ARMOR_JACKET,
    ITEM_ARMOR_SHARD,
    ITEM_POWER_SCREEN,
    ITEM_POWER_SHIELD,
    ITEM_BLASTER,
    ITEM_SHOTGUN,
    ITEM_SUPERSHOTGUN,
    ITEM_MACHINEGUN,
    ITEM_CHAINGUN,
    ITEM_GRENADES,
    ITEM_GRENADELAUNCHER,
    ITEM_ROCKETLAUNCHER,
    ITEM_HYPERBLASTER,
    ITEM_RAILGUN,
    ITEM_BFG,
    ITEM_SHELLS,
    ITEM_BULLETS,
    ITEM_CELLS,
    ITEM_ROCKETS,
    ITEM_SLUGS,
    ITEM_QUAD,
    ITEM_INVULNERABILITY,
    ITEM_SILENCER,
    ITEM_BREATHER,
    ITEM_ENVIRO,
    ITEM_ANCIENT_HEAD,
    ITEM_ADRENALINE,
    ITEM_BANDOLIER,
    ITEM_PACK,
    ITEM_HEALTH,

    ITEM_TOTAL
} item_t;

//deadflag
#define DEAD_NO                 0
#define DEAD_DYING              1
#define DEAD_DEAD               2
#define DEAD_RESPAWNABLE        3

//range
#define RANGE_MELEE             0
#define RANGE_NEAR              1
#define RANGE_MID               2
#define RANGE_FAR               3

//gib types
#define GIB_ORGANIC             0
#define GIB_METALLIC            1

//monster ai flags
#define AI_STAND_GROUND         0x00000001
#define AI_TEMP_STAND_GROUND    0x00000002
#define AI_SOUND_TARGET         0x00000004
#define AI_LOST_SIGHT           0x00000008
#define AI_PURSUIT_LAST_SEEN    0x00000010
#define AI_PURSUE_NEXT          0x00000020
#define AI_PURSUE_TEMP          0x00000040
#define AI_HOLD_FRAME           0x00000080
#define AI_GOOD_GUY             0x00000100
#define AI_BRUTAL               0x00000200
#define AI_NOSTEP               0x00000400
#define AI_DUCKED               0x00000800
#define AI_COMBAT_POINT         0x00001000
#define AI_MEDIC                0x00002000
#define AI_RESURRECTING         0x00004000

//monster attack state
#define AS_STRAIGHT             1
#define AS_SLIDING              2
#define AS_MELEE                3
#define AS_MISSILE              4

// armor types
#define ARMOR_NONE              0
#define ARMOR_JACKET            1
#define ARMOR_COMBAT            2
#define ARMOR_BODY              3
#define ARMOR_SHARD             4

// power armor types
//#define POWER_ARMOR_NONE        0
//#define POWER_ARMOR_SCREEN      1
//#define POWER_ARMOR_SHIELD      2

// handedness values
#define RIGHT_HANDED            0
#define LEFT_HANDED             1
#define CENTER_HANDED           2

// game.serverflags values
#define SFL_CROSS_TRIGGER_1     0x00000001
#define SFL_CROSS_TRIGGER_2     0x00000002
#define SFL_CROSS_TRIGGER_3     0x00000004
#define SFL_CROSS_TRIGGER_4     0x00000008
#define SFL_CROSS_TRIGGER_5     0x00000010
#define SFL_CROSS_TRIGGER_6     0x00000020
#define SFL_CROSS_TRIGGER_7     0x00000040
#define SFL_CROSS_TRIGGER_8     0x00000080
#define SFL_CROSS_TRIGGER_MASK  0x000000ff

// edict->movetype values
typedef enum {
    MOVETYPE_NONE,          // never moves
    MOVETYPE_NOCLIP,        // origin and angles change with no interaction
    MOVETYPE_PUSH,          // no clip to world, push on box contact
    MOVETYPE_STOP,          // no clip to world, stops on box contact

    MOVETYPE_WALK,          // gravity
    MOVETYPE_STEP,          // gravity, special edge handling
    MOVETYPE_FLY,
    MOVETYPE_TOSS,          // gravity
    MOVETYPE_FLYMISSILE,    // extra size to monsters
    MOVETYPE_BOUNCE
} movetype_t;


typedef struct {
    int     base_count;
    int     max_count;
    float   normal_protection;
    float   energy_protection;
    int     armor;
} gitem_armor_t;


// gitem_t->flags
#define IT_WEAPON       1       // use makes active weapon
#define IT_AMMO         2
#define IT_ARMOR        4
//#define IT_STAY_COOP    8
#define IT_KEY          16
#define IT_POWERUP      32

// gitem_t->weapmodel for weapons indicates model index
#define WEAP_NONE               0

#define WEAP_BLASTER            1
#define WEAP_SHOTGUN            2
#define WEAP_SUPERSHOTGUN       3
#define WEAP_MACHINEGUN         4
#define WEAP_CHAINGUN           5
#define WEAP_GRENADES           6
#define WEAP_GRENADELAUNCHER    7
#define WEAP_ROCKETLAUNCHER     8
#define WEAP_HYPERBLASTER       9
#define WEAP_RAILGUN            10
#define WEAP_BFG                11

#define WEAP_TOTAL              12

typedef struct gitem_s {
    char        *classname; // spawning name
    qboolean    (*pickup)(struct edict_s *ent, struct edict_s *other);
    void        (*use)(struct edict_s *ent, struct gitem_s *item);
    void        (*drop)(struct edict_s *ent, struct gitem_s *item);
    void        (*weaponthink)(struct edict_s *ent);
    char        *pickup_sound;
    char        *world_model;
    int         world_model_flags;
    char        *view_model;

    // client side info
    char        *icon;
    char        *pickup_name;   // for printing on pickup
    int         count_width;    // number of digits to display by icon

    int         quantity;       // for ammo how much, for weapons how much is used per shot
    char        *ammo;          // for weapons
    int         flags;          // IT_* flags

    int         weapmodel;      // weapon model index (for weapons)

    const void  *info;
    int         tag;

    char        *precaches;     // string of all models, sounds, and images this item will use
} gitem_t;

typedef struct skin_entry_s {
    struct skin_entry_s *next;
    struct skin_entry_s *down;
    char name[1];
} skin_entry_t;

//
// this structure is left intact through an entire game
// it should be initialized at dll load time, and read/written to
// the server.ssv file for savegames
//
typedef struct {
    gclient_t   *clients;       // [maxclients]

    // scoreboard layout from previous level
    char        oldscores[MAX_STRING_CHARS];

    // store latched cvars here that we want to get at often
    int         maxclients;
    int         maxentities;

    // cross level triggers
    int         serverflags;
    int         serverFeatures;

#if USE_FPS
    int         framerate;
    float       frametime;
    int         framediv;
#endif

    int         settings_modified;

    // allowed skins list
    skin_entry_t    *skins;

    char        dir[MAX_OSPATH]; // where variable data is stored
} game_locals_t;

// vote scopes
#define VOTE_SCOPE_NONE     (-1)    // no vote
#define VOTE_SCOPE_GLOBAL   (0)     // vote affects the whole server
#define VOTE_SCOPE_ARENA    (1)     // vote affects only one arena

// vote proposals
#define VOTE_KICK       (1 << 0)    // global
#define VOTE_MUTE       (1 << 1)    // global
#define VOTE_MAP        (1 << 2)    // global
#define VOTE_TEAMS      (1 << 3)    // local (arena)
#define VOTE_WEAPONS    (1 << 4)    // local
#define VOTE_DAMAGE     (1 << 5)    // local
#define VOTE_ROUNDS     (1 << 6)    // local
#define VOTE_HEALTH     (1 << 7)    // local
#define VOTE_ARMOR      (1 << 8)    // local
#define VOTE_RESET      (1 << 9)    // local
#define VOTE_SWITCH     (1 << 10)   // local
#define VOTE_ALL        (VOTE_KICK | VOTE_MUTE | VOTE_MAP | \
                        VOTE_TEAMS | VOTE_WEAPONS | VOTE_DAMAGE | \
                        VOTE_ROUNDS | VOTE_HEALTH | VOTE_ARMOR | \
                        VOTE_RESET | VOTE_SWITCH)

// vote flags
#define VF_ENABLED  1       // globally allow voting on this server
#define VF_SHOW     2       // show the vote in the player hud
#define VF_SPECS    4       // allow spectators to vote
#define VF_CHANGE   8       // allow users to change their vote
#define VF_DEFAULT  (VF_ENABLED | VF_SHOW | VF_CHANGE)

#define VF(x)       (((int)g_vote_flags->value & VF_##x) != 0)

#define MAX_SPAWNS  64

#define ITB_QUAD    1
#define ITB_INVUL   2
#define ITB_BFG     4
#define ITB_PS      8

#define MAX_SCORES  10


typedef struct {
    char name[MAX_NETNAME];
    int score;
    time_t time;
} score_t;

typedef struct map_entry_s {
    list_t  list;
    list_t  queue;
    int     min_players;
    int     max_players;
    int     flags;
    float   weight;
    int     num_hits, num_in, num_out; // for statistics
    char    name[1];
	arena_entry_t arenas[MAX_ARENAS];
	char	extra_ents[MAX_STRING_CHARS];	// additional entities from <mapname>.ent file
} map_entry_t;

//
// this structure is cleared as each map is entered
//
typedef struct {
    int         framenum;
    float       time;

    char        mapname[MAX_QPATH];     // the server name (base1, etc)
    char        nextmap[MAX_QPATH];     // go here when fraglimit is hit
    const char  *entstring;

    edict_t     *spawns[MAX_SPAWNS];
    int         numspawns;

#if 0
//  int         status;
    int         warmup_framenum;        // time the warmup was started
    int         countdown_framenum;     // time the countdown was started
    int         match_framenum;         // time the match was started
    int         pause_framenum;         // time the pause was started

//  int         frames_remaining;       // timelimit
#endif
    int         activity_framenum;      // time the last client has been active

    // intermission state
    int         intermission_framenum;      // time the intermission was started
    int         intermission_exit;          // time the intermission was exited
    vec3_t      intermission_origin;
    vec3_t      intermission_angle;

    score_t     scores[MAX_SCORES];
    int         numscores;
    time_t      record;        // not zero if scores updated

    int         players_in;
    int         players_out;

    struct {
        int     health;
        int     powershield;
        int     quad;
        int     invulnerability;
        int     envirosuit;
        int     rebreather;
    } images;

    struct {
        int     fry;
        int     lava_in;
        int     watr_in;
        int     watr_out;
        int     watr_un;
        int     burn[2];
        int     drown;
        int     breath[2];
        int     gasp[2];
        int     udeath;
        int     windfly;

        int     rg_hum;
        int     bfg_hum;
        int     noammo;

        int     death[4];
        int     fall[2];
        int     gurp[2];
        int     jump;
        int     pain[4][2];

        int     secret;
        int     count;
        int     xian;
        int     makron;
        int     teleport;
        int     countdown[15];
        int     fight[7];
        int     round[22];
        int     finalround;
        int     timeout;
        int     timein;
        int     winner;
        int     loser;
        int     yousuck;
        int     impressive;
        int     flawless;
        int     horn;
    } sounds;

    struct {
        int     meat;
        int     skull;
        int     head;
        int     arm, leg, chest, bones[2];
    } models;

    struct {
        int     proposal;
        int     index;
        int     framenum;

        int     value;
        struct gclient_s    *victim;
        struct gclient_s    *initiator;
        char    map[MAX_QPATH];
    } vote;

    edict_t     *current_entity;    // entity running from G_RunFrame
    int         body_que;           // dead bodies
	
    arena_t     arenas[MAX_ARENAS];
    int         arena_count;
    int         default_arena;
    arena_entry_t arena_defaults[MAX_ARENAS];

    map_entry_t *map;
} level_locals_t;


// spawn_temp_t is only used to hold entity field values that
// can be set from the editor, but aren't actualy present
// in edict_t during gameplay
typedef struct {
    // world vars
    char        *sky;
    float       skyrotate;
    vec3_t      skyaxis;
    char        *nextmap;

    int         lip;
    int         distance;
    int         height;
    char        *noise;
    float       pausetime;
    char        *item;
    char        *gravity;

    float       minyaw;
    float       maxyaw;
    float       minpitch;
    float       maxpitch;
} spawn_temp_t;


typedef struct {
    // fixed data
    vec3_t      start_origin;
    vec3_t      start_angles;
    vec3_t      end_origin;
    vec3_t      end_angles;

    int         sound_start;
    int         sound_middle;
    int         sound_end;

    float       accel;
    float       speed; // in units per frame
    float       decel;
    float       distance;

    int         wait; // in frames

    // state data
    int         state;
    vec3_t      dir;
    float       current_speed;
    float       move_speed;
    float       next_speed;
    float       remaining_distance;
    float       decel_distance;
    void        (*endfunc)(edict_t *);
} moveinfo_t;


#define MAP_NOAUTO      1
#define MAP_NOVOTE      2
#define MAP_EXCLUSIVE   0x40000000
#define MAP_WEIGHTED    0x80000000


struct flood_s;

extern  const gitem_t   g_itemlist[ITEM_TOTAL];

extern  game_locals_t   game;
extern  level_locals_t  level;
extern  game_import_t   gi;
extern  game_export_t   globals;
extern  spawn_temp_t    st;

extern	pmenu_arena_t	menu_lookup[MAX_ARENAS];

// means of death
#define MOD_UNKNOWN         0
#define MOD_BLASTER         1
#define MOD_SHOTGUN         2
#define MOD_SSHOTGUN        3
#define MOD_MACHINEGUN      4
#define MOD_CHAINGUN        5
#define MOD_GRENADE         6
#define MOD_G_SPLASH        7
#define MOD_ROCKET          8
#define MOD_R_SPLASH        9
#define MOD_HYPERBLASTER    10
#define MOD_RAILGUN         11
#define MOD_BFG_LASER       12
#define MOD_BFG_BLAST       13
#define MOD_BFG_EFFECT      14
#define MOD_HANDGRENADE     15
#define MOD_HG_SPLASH       16
#define MOD_WATER           17
#define MOD_SLIME           18
#define MOD_LAVA            19
#define MOD_CRUSH           20
#define MOD_TELEFRAG        21
#define MOD_FALLING         22
#define MOD_SUICIDE         23
#define MOD_HELD_GRENADE    24
#define MOD_EXPLOSIVE       25
#define MOD_BARREL          26
#define MOD_BOMB            27
#define MOD_EXIT            28
#define MOD_SPLASH          29
#define MOD_TARGET_LASER    30
#define MOD_TRIGGER_HURT    31
#define MOD_HIT             32
#define MOD_TARGET_BLASTER  33
#define MOD_TOTAL           34
#define MOD_FRIENDLY_FIRE   0x8000000

extern  int         meansOfDeath;


extern  edict_t     *g_edicts;

#define FOFS(x)     q_offsetof(edict_t, x)
#define STOFS(x)    q_offsetof(spawn_temp_t, x)

#define random()    ((rand () & 0x7fff) / ((float)0x7fff))
#define crandom()   (2.0 * (random() - 0.5))

#define DF(x)       (((int)dmflags->value & DF_##x) != 0)

extern  cvar_t  *maxentities;
extern  cvar_t  *dmflags;
extern  cvar_t  *skill;
extern  cvar_t  *timelimit;
extern  cvar_t  *g_select_empty;
extern  cvar_t  *g_idle_time;

extern  cvar_t  *g_vote_mask;
extern  cvar_t  *g_vote_time;
extern  cvar_t  *g_vote_threshold;
extern  cvar_t  *g_vote_limit;
extern  cvar_t  *g_vote_flags;

extern	cvar_t	*g_frag_drop;
extern	cvar_t	*g_team_balance;
extern	cvar_t	*g_team_count;
extern	cvar_t	*g_teamspec_name;
extern 	cvar_t	*g_team1_name;
extern	cvar_t	*g_team1_skin;
extern 	cvar_t	*g_team2_name;
extern	cvar_t	*g_team2_skin;
extern 	cvar_t	*g_team3_name;
extern	cvar_t	*g_team3_skin;
extern 	cvar_t	*g_team4_name;
extern	cvar_t	*g_team4_skin;
extern 	cvar_t	*g_team5_name;
extern	cvar_t	*g_team5_skin;
extern	cvar_t	*g_round_end_time;
extern	cvar_t	*g_default_arena;
extern	cvar_t	*g_round_limit;
extern	cvar_t	*g_timeout_time;
extern  cvar_t  *g_intermission_time;
extern  cvar_t  *g_admin_password;
extern  cvar_t  *g_item_ban;
extern  cvar_t  *g_maps_random;
extern  cvar_t  *g_bugs;
extern  cvar_t  *g_teleporter_nofreeze;
extern  cvar_t  *g_spawn_mode;
extern  cvar_t  *g_team_chat;
extern  cvar_t  *g_mute_chat;
extern  cvar_t  *g_protection_time;
extern  cvar_t  *dedicated;

#if USE_SQLITE
extern  cvar_t  *g_sql_database;
extern  cvar_t  *g_sql_async;
#endif

extern  cvar_t  *sv_gravity;
extern  cvar_t  *sv_maxvelocity;

extern  cvar_t  *gun_x, *gun_y, *gun_z;
extern  cvar_t  *sv_rollspeed;
extern  cvar_t  *sv_rollangle;

extern  cvar_t  *run_pitch;
extern  cvar_t  *run_roll;
extern  cvar_t  *bob_up;
extern  cvar_t  *bob_pitch;
extern  cvar_t  *bob_roll;

extern  cvar_t  *sv_cheats;
extern  cvar_t  *sv_hostname;
extern  cvar_t  *maxclients;

extern  cvar_t  *flood_msgs;
extern  cvar_t  *flood_persecond;
extern  cvar_t  *flood_waitdelay;

extern  cvar_t  *flood_waves;
extern  cvar_t  *flood_perwave;
extern  cvar_t  *flood_wavedelay;

extern  cvar_t  *flood_infos;
extern  cvar_t  *flood_perinfo;
extern  cvar_t  *flood_infodelay;

extern	cvar_t	*g_round_countdown;
extern	cvar_t	*g_weapon_flags;
extern	cvar_t	*g_damage_flags;
extern	cvar_t	*g_health_start;
extern	cvar_t	*g_armor_start;
extern	cvar_t	*g_drop_allowed;
extern	cvar_t	*g_skin_lock;
extern	cvar_t	*g_ammo_slugs;
extern	cvar_t	*g_ammo_rockets;
extern	cvar_t	*g_ammo_cells;
extern	cvar_t	*g_ammo_grenades;
extern	cvar_t	*g_ammo_bullets;
extern	cvar_t	*g_ammo_shells;
extern	cvar_t	*g_timein_time;
extern	cvar_t	*g_screenshot;
extern	cvar_t	*g_demo;
extern	cvar_t	*g_team_reset;
extern	cvar_t	*g_all_chat;
extern  cvar_t  *g_round_timelimit;
extern  cvar_t  *g_fast_weapon_change;

extern  list_t  g_map_list;
extern  list_t  g_map_queue;

//extern  cvar_t  *sv_features;

#define world   (&g_edicts[0])

// item spawnflags
#define ITEM_TRIGGER_SPAWN      0x00000001
#define ITEM_NO_TOUCH           0x00000002
// 6 bits reserved for editor flags
// 8 bits used as power cube id bits for coop games
#define DROPPED_ITEM            0x00010000
#define DROPPED_PLAYER_ITEM     0x00020000
#define ITEM_TARGETS_USED       0x00040000


//
// g_cmds.c
//
void Cmd_Players_f(edict_t *ent);
void Cmd_HighScores_f(edict_t *ent);
void Cmd_Stats_f(edict_t *ent, qboolean check_other);
void Cmd_Settings_f(edict_t *ent);
edict_t *G_SetPlayer(edict_t *ent, int arg);
edict_t *G_SetVictim(edict_t *ent, int start);
void ValidateSelectedItem(edict_t *ent);
qboolean G_FloodProtect(edict_t *ent, struct flood_s *flood,
                        const char *what, int msgs, float persecond, float delay);
void Cmd_ArenaMenu_f(edict_t *ent);
void Cmd_Menu_f(edict_t *ent);

//
// g_items.c
//
#define ITEM_INDEX(x) ((x) - g_itemlist)
#define INDEX_ITEM(x) ((gitem_t *)&g_itemlist[(x)])

void PrecacheItem(gitem_t *it);
void InitItems(void);
void SetItemNames(void);
gitem_t *FindItem(char *pickup_name);
gitem_t *FindItemByClassname(char *classname);
edict_t *Drop_Item(edict_t *ent, gitem_t *item);
void SetRespawn(edict_t *ent, float delay);
void ChangeWeapon(edict_t *ent);
void SpawnItem(edict_t *ent, gitem_t *item);
void Think_Weapon(edict_t *ent);
int ArmorIndex(edict_t *ent);
int PowerArmorIndex(edict_t *ent);
qboolean Add_Ammo(edict_t *ent, gitem_t *item, int count);
void Touch_Item(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf);
void G_UpdateItemBans(void);

//
// g_utils.c
//
qboolean    G_KillBox(edict_t *ent);
void    G_ProjectSource(vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result);
edict_t *G_Find(edict_t *from, size_t fieldofs, char *match);
edict_t *findradius(edict_t *from, vec3_t org, float rad);
edict_t *G_PickTarget(char *targetname);
void    G_UseTargets(edict_t *ent, edict_t *activator);
void    G_SetMovedir(vec3_t angles, vec3_t movedir);

void    G_InitEdict(edict_t *e);
edict_t *G_Spawn(void);
void    G_FreeEdict(edict_t *e);

void    G_TouchTriggers(edict_t *ent);
//void  G_TouchSolids (edict_t *ent);

void    G_ShuffleArray(void *base, size_t n);

size_t  G_HighlightStr(char *dst, const char *src, size_t size);

#define G_Malloc(x) gi.TagMalloc(x, TAG_GAME)
char    *G_CopyString(const char *in);

float   *tv(float x, float y, float z);
char    *vtos(vec3_t v);

float vectoyaw(vec3_t vec);
void vectoangles(vec3_t vec, vec3_t angles);
qboolean match(char *pattern, char *haystack);

//
// g_combat.c
//
qboolean OnSameTeam(edict_t *ent1, edict_t *ent2);
qboolean CanDamage(edict_t *targ, edict_t *inflictor);
void T_Damage(edict_t *targ, edict_t *inflictor, edict_t *attacker, vec3_t dir, vec3_t point, vec3_t normal, int damage, int knockback, int dflags, int mod);
void T_RadiusDamage(edict_t *inflictor, edict_t *attacker, float damage, edict_t *ignore, float radius, int mod);

// damage flags
#define DAMAGE_RADIUS           0x00000001  // damage was indirect
#define DAMAGE_NO_ARMOR         0x00000002  // armour does not protect from this damage
#define DAMAGE_ENERGY           0x00000004  // damage is from an energy based weapon
#define DAMAGE_NO_KNOCKBACK     0x00000008  // do not affect velocity, just view angles
#define DAMAGE_BULLET           0x00000010  // damage is from a bullet (used for ricochets)
#define DAMAGE_NO_PROTECTION    0x00000020  // armor, shields, invulnerability, and godmode have no effect

#define DEFAULT_BULLET_HSPREAD  300
#define DEFAULT_BULLET_VSPREAD  500
#define DEFAULT_SHOTGUN_HSPREAD 1000
#define DEFAULT_SHOTGUN_VSPREAD 500
#define DEFAULT_DEATHMATCH_SHOTGUN_COUNT    12
#define DEFAULT_SHOTGUN_COUNT   12
#define DEFAULT_SSHOTGUN_COUNT  20

//
// g_misc.c
//
void ThrowHead(edict_t *self, int modelindex, int damage, int type);
void ThrowClientHead(edict_t *self, int damage);
void ThrowGib(edict_t *self, int modelindex, int damage, int type);
void BecomeExplosion1(edict_t *self);

//
// g_weapon.c
//
qboolean fire_hit(edict_t *self, vec3_t aim, int damage, int kick);
void fire_bullet(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int mod);
void fire_shotgun(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int count, int mod);
void fire_blaster(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int effect, qboolean hyper);
void fire_grenade(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int timer, float damage_radius);
void fire_grenade2(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int timer, float damage_radius, qboolean held);
void fire_rocket(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius, int radius_damage);
void fire_rail(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick);
void fire_bfg(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius);

//
// p_client.c
//
void respawn(edict_t *ent);
void spectator_respawn(edict_t *ent, int connected);
void BeginIntermission(arena_t *a);
void PutClientInServer(edict_t *ent);
void InitBodyQue(void);
void ClientBeginServerFrame(edict_t *ent);
void G_WriteTime(int remaining);
void G_BeginDamage(void);
void G_AccountDamage(edict_t *targ, edict_t *inflictor, edict_t *attacker, int points);
void G_EndDamage(void);
void G_SetDeltaAngles(edict_t *ent, vec3_t angles);
void G_ScoreChanged(edict_t *ent);
int G_UpdateRanks(void);
edict_t *SelectIntermissionPoint(arena_t *a);
void ClientString(edict_t *ent, uint16_t index, const char *str);



//
// g_player.c
//
void player_pain(edict_t *self, edict_t *other, float kick, int damage);
void player_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point);

//
// g_svcmds.c
//
void G_ServerCommand(void);

//
// p_view.c
//
void ClientEndServerFrame(edict_t *ent);
void IntermissionEndServerFrame(edict_t *ent);

//
// p_hud.c
//
void MoveClientToIntermission(edict_t *client);
void G_PrivateString(edict_t *ent, int index, const char *string);
int G_GetPlayerIdView(edict_t *ent, qboolean *teammate);
void G_SetStats(edict_t *ent);
int G_CalcRanks(gclient_t **ranks);
void ScoreboardMessage(edict_t *ent, qboolean reliable);
void HighScoresMessage(void);

//
// g_phys.c
//
void G_RunEntity(edict_t *ent);

//
// g_main.c
//
void SaveClientData(void);
void FetchClientEntData(edict_t *ent);
void G_ExitLevel(void);
void G_StartSound(int index);
void G_StuffText(edict_t *ent, const char *text);
void G_RunFrame(void);
void G_LoadScores(void);
map_entry_t *G_FindMap(const char *name);

//
// g_spawn.c
//
void G_SpawnEntities(const char *mapname, const char *entities, const char *spawnpoint);
void G_ResetLevel(void);
qboolean G_ParseDamageString(arena_t *a, edict_t *ent, const char **input, uint32_t *target);
qboolean G_ParseWeaponString(arena_t *arena, edict_t *ent, const char **input, temp_weaponflags_t *t);

/**
 * g_random.c
 */
void init_genrand(unsigned long s);
void init_by_array(unsigned long init_key[], int key_length);
unsigned long genrand_int32(void);
long genrand_int31(void);
double genrand_float32_full(void);
double genrand_float32_notone(void);


// private strings (unicast configstrings)
#define PCS_DAMAGE	0
#define PCS_DELTA	1
#define PCS_RANK	2
#define PCS_VOTE	3
#define PCS_TOTAL	4

// configstring indexes
#define CS_OBSERVE          (CS_GENERAL + 1)
#define CS_TIME             (CS_GENERAL + 2)
#define CS_SPECMODE         (CS_GENERAL + 3)
#define CS_PREGAME          (CS_GENERAL + 4)
#define CS_VOTE_PROPOSAL    (CS_GENERAL + 5)
#define CS_VOTE_COUNT       (CS_GENERAL + 6)
#define CS_READY            (CS_GENERAL + 7)
#define CS_READY_WAIT       (CS_GENERAL + 8)
#define CS_READY_BALANCED   (CS_GENERAL + 9)
#define CS_MATCH_STATUS     (CS_GENERAL + 10)
#define CS_ARENA_TIMEOUT    (CS_GENERAL + 11)
#define CS_ROUND            (CS_GENERAL + 12)
#define CS_ARENA_COUNTDOWN  (CS_GENERAL + 13)
#define CS_PLAYERNAMES      (CS_GENERAL + 14)
#define CS_PRIVATE          (CS_PLAYERNAMES + MAX_CLIENTS)

// playerstate->stat[] indexes (0-31)
#define STAT_HEALTH_ICON        0
#define STAT_HEALTH             1   // used by client, server
#define STAT_AMMO_ICON          2
#define STAT_AMMO               3   // used by client
#define STAT_ARMOR_ICON         4
#define STAT_ARMOR              5   // used by client
#define STAT_SELECTED_ICON      6   // ?
#define STAT_WEAPON_ICON        7   // the current gun
#define STAT_VIEWID_TEAM        8
#define STAT_TIMER_ICON         9
#define STAT_TIMER              10
#define STAT_ROUNDTIME          11  // old STAT_HELPICON
#define STAT_SELECTED_ITEM      12  // used by client
#define STAT_LAYOUTS            13  // used by client
#define STAT_FRAGS              14  // used by server
#define STAT_FLASHES            15  // used by client, cleared each frame, 1 = health, 2 = armor
#define STAT_CHASE              16
#define STAT_SPECTATOR          17  // ? possibly use different hud
#define STAT_AMMO_SHELLS        18
#define STAT_AMMO_BULLETS       19
#define STAT_AMMO_GRENADES      20
#define STAT_AMMO_CELLS         21
#define STAT_AMMO_ROCKETS       22
#define STAT_AMMO_SLUGS         23
#define STAT_VIEWID             24
#define STAT_VOTE_PROPOSAL      25
#define STAT_VOTE_COUNT         26
#define STAT_TIMEOUT            27  // remove
#define STAT_ROUND              28
#define STAT_COUNTDOWN          29
#define STAT_READY              30
#define STAT_MATCH_STATUS       31  // warmup, time, timeout, countdown, etc

// client_t->anim_priority
#define ANIM_BASIC      0       // stand / run
#define ANIM_WAVE       1
#define ANIM_JUMP       2
#define ANIM_PAIN       3
#define ANIM_ATTACK     4
#define ANIM_DEATH      5
#define ANIM_REVERSE    6

#define PLAYER_SPAWNED(e) \
    ((e)->client->pers.connected == CONN_SPAWNED)

#define MAX_MENU_ENTRIES    18

typedef enum {
    PMENU_ALIGN_LEFT,
    PMENU_ALIGN_CENTER,
    PMENU_ALIGN_RIGHT
} pmenu_align_t;

struct pmenu_s;
typedef void (*pmenu_select_t)(struct edict_s *);

typedef struct pmenu_entry_s {
    char *text;
    pmenu_align_t align;
    pmenu_select_t select;
} pmenu_entry_t;

typedef struct pmenu_s {
    int cur;
    pmenu_entry_t entries[MAX_MENU_ENTRIES];
} pmenu_t;

typedef enum {
    CONN_DISCONNECTED,
    CONN_CONNECTED,
    CONN_PREGAME,
    CONN_SPAWNED,
    CONN_SPECTATOR
} conn_t;

typedef enum {
    GENDER_FEMALE,
    GENDER_MALE,
    GENDER_NEUTRAL
} gender_t;

typedef enum {
    GRENADE_NONE,
    GRENADE_BLEW_UP,
    GRENADE_THROWN,
} grenade_state_t;

typedef enum {
    CHASE_NONE,
    CHASE_LEADER,
    CHASE_QUAD,
    CHASE_INVU
} chase_mode_t;

typedef enum {
    LAYOUT_NONE,
    LAYOUT_SCORES,
    LAYOUT_OLDSCORES,
    LAYOUT_MENU,
    LAYOUT_PLAYERS
} layout_t;

typedef enum {
    FRAG_UNKNOWN,
    FRAG_BLASTER,
    FRAG_SHOTGUN,
    FRAG_SUPERSHOTGUN,
    FRAG_MACHINEGUN,
    FRAG_CHAINGUN,
    FRAG_GRENADES,
    FRAG_GRENADELAUNCHER,
    FRAG_ROCKETLAUNCHER,
    FRAG_HYPERBLASTER,
    FRAG_RAILGUN,
    FRAG_BFG,
    FRAG_TELEPORT,
    FRAG_WATER,
    FRAG_SLIME,
    FRAG_LAVA,
    FRAG_CRUSH,
    FRAG_FALLING,
    FRAG_SUICIDE,
    FRAG_TOTAL
} frag_t;

typedef struct {
    int kills;
    int deaths;
    int suicides;
    int hits;
    int atts;
} fragstat_t;

typedef struct {
    int pickups;
    int misses;
    int kills;
} itemstat_t;

#define FLOOD_MSGS  10
typedef struct flood_s {
    int     locktill;           // locked from talking
    int     when[FLOOD_MSGS];   // when messages were said
    int     whenhead;           // head pointer for when said
} flood_t;

// client data that stays across multiple level loads
typedef struct {
    char        	netname[MAX_NETNAME];
    char        	skin[MAX_SKINNAME];
    char        	ip[32];
    int         	hand;
    float       	fov;
    gender_t    	gender;
    int         	uf;
    conn_t      	connected;
    qboolean        loopback: 1,
                    mvdspec: 1,
                    admin: 1,
                    noviewid: 1,
                    muted: 1;
    qboolean        ready;
    arena_t         *arena;
    arena_team_t    *team;
} client_persistant_t;

// client data that stays across deathmatch respawns,
// but cleared on spectator respawns
typedef struct {
    int         enter_framenum;     // level.framenum the client entered the game
    int         activity_framenum;  // level.framenum last activity was seen
    int         score;              // frags, etc
	int			round_score;		// rounds won
	int			match_score;		// matches won
    int         deaths;
    fragstat_t  frags[FRAG_TOTAL];
    itemstat_t  items[ITEM_TOTAL];
    int         damage_given, damage_recvd;
} client_respawn_t;

// client data that stays across respawns,
// but cleared on level changes
typedef struct {
    qboolean    first_time : 1,     // true when just connected
                jump_held: 1;
    vec3_t      cmd_angles;         // angles sent over in the last command
    char        strings[PCS_TOTAL][MAX_NETNAME]; // private configstrings
    struct {
        int         index;
        qboolean    accepted;
        int         count;
    } vote;
    flood_t     chat_flood, wave_flood, info_flood;
} client_level_t;

// this structure is cleared on each PutClientInServer(),
// except for 'client->pers'
struct gclient_s {
    // known to server
    player_state_t  ps;             // communicated by server to clients
    int             ping;

    // known to compatible server
    int             clientNum;

    // private to game
    client_persistant_t pers;
    client_respawn_t    resp;
    client_level_t      level;
    pmove_state_t       old_pmove;  // for detecting out-of-pmove changes

    edict_t         *edict;

    layout_t    layout;         // set layout stat

    pmenu_t     menu;
//  int         menu_framenum;
    qboolean    menu_dirty;

    int         ammo_index;

    int         buttons;
    int         oldbuttons;
    int         latched_buttons;

    qboolean    weapon_thunk;

    gitem_t     *newweapon;

    // sum up damage over an entire frame, so
    // shotgun blasts give a single big kick
    int         damage_armor;       // damage absorbed by armor
    int         damage_parmor;      // damage absorbed by power armor
    int         damage_blood;       // damage taken out of health
    int         damage_knockback;   // impact damage
    vec3_t      damage_from;        // origin for vector calculation

    float       killer_yaw;         // when dead, look at killer

    weaponstate_t   weaponstate;
    int             weaponframe;
    vec3_t      kick_angles;    // weapon kicks
    vec3_t      kick_origin;
    float       v_dmg_roll, v_dmg_pitch, v_dmg_time;    // damage kicks
    float       fall_time, fall_value;      // for view drop on fall
    float       damage_alpha;
    float       bonus_alpha;
    vec3_t      damage_blend;
    vec3_t      v_angle;            // aiming direction
    float       bobtime;            // so off-ground doesn't change it
    vec3_t      oldviewangles;
    vec3_t      oldvelocity;

    int         next_drown_framenum;
    int         old_waterlevel;
    int         breather_sound;

    int         machinegun_shots;   // for weapon raising

    // animation vars
    int         anim_start;
    int         anim_end;
    int         anim_priority;
    qboolean    anim_duck;
    qboolean    anim_run;

    // powerup timers
    int         quad_framenum;
    int         invincible_framenum;
    int         breather_framenum;
    int         enviro_framenum;
    int         powerarmor_framenum;

    grenade_state_t grenade_state;
    int             grenade_framenum;
    int         silencer_shots;
    int         weapon_sound;

    int         pickup_framenum;

    int         respawn_framenum;   // can respawn when time > this

    edict_t         *chase_target;      // player we are chasing
    chase_mode_t    chase_mode;

    int         selected_item;
    int         inventory[MAX_ITEMS];

    // ammo capacities
    int         max_bullets;
    int         max_shells;
    int         max_rockets;
    int         max_grenades;
    int         max_cells;
    int         max_slugs;

    gitem_t     *weapon;
    gitem_t     *lastweapon;
};


struct edict_s {
    entity_state_t  s;
    struct gclient_s    *client;    // NULL if not a player
                                    // the server expects the first part
                                    // of gclient_s to be a player_state_t
                                    // but the rest of it is opaque

    qboolean    inuse;
    int         linkcount;

    // FIXME: move these fields to a server private sv_entity_t
    list_t      area;               // linked to a division node or leaf

    int         num_clusters;       // if -1, use headnode instead
    int         clusternums[MAX_ENT_CLUSTERS];
    int         headnode;           // unused if num_clusters != -1
    int         areanum, areanum2;

    //================================

    int         svflags;
    vec3_t      mins, maxs;
    vec3_t      absmin, absmax, size;
    solid_t     solid;
    int         clipmask;
    edict_t     *owner;


    // DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
    // EXPECTS THE FIELDS IN THAT ORDER!

    //================================
    int         movetype;
    int         flags;

    char        *model;
    float       freetime;           // sv.time when the object was freed

    //
    // only used locally in game, not by server
    //
    char        *message;
    char        *classname;
    int         spawnflags;

    float       timestamp;

    float       angle;          // set in qe3, -1 = up, -2 = down
    char        *target;
    char        *targetname;
    char        *killtarget;
    char        *team;
    char        *pathtarget;
    char        *deathtarget;
    char        *combattarget;
    edict_t     *target_ent;

    float       speed, accel, decel;
    vec3_t      movedir;
    vec3_t      pos1, pos2;

    vec3_t      velocity;
    vec3_t      avelocity;
    int         mass;
    int         air_finished_framenum;
    float       gravity;        // per entity gravity multiplier (1.0 is normal)
                                // use for lowgrav artifact, flares

    edict_t     *goalentity;
    edict_t     *movetarget;
    float       yaw_speed;
    float       ideal_yaw;

    int         nextthink;
    void        (*prethink)(edict_t *ent);
    void        (*think)(edict_t *self);
    void        (*blocked)(edict_t *self, edict_t *other);         //move to moveinfo?
    void        (*touch)(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf);
    void        (*use)(edict_t *self, edict_t *other, edict_t *activator);
    void        (*pain)(edict_t *self, edict_t *other, float kick, int damage);
    void        (*die)(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point);

    int         touch_debounce_framenum;        // are all these legit?  do we need more/less of them?
    int         pain_debounce_framenum;
    int         fly_sound_debounce_framenum;    //move to clientinfo
    float       last_move_time;

    int         health;
    int         max_health;
    int         gib_health;
    int         deadflag;
    qboolean    show_hostile;

    char        *map;           // target_changelevel

    int         viewheight;     // height above origin where eyesight is determined
    int         takedamage;
    int         dmg;
    int         radius_dmg;
    float       dmg_radius;
    int         sounds;         //make this a spawntemp var?
    int         count;

    edict_t     *chain;
    edict_t     *enemy;
    edict_t     *oldenemy;
    edict_t     *activator;
    edict_t     *groundentity;
    int         groundentity_linkcount;
    edict_t     *teamchain;
    edict_t     *teammaster;

    edict_t     *mynoise;       // can go in client only
    edict_t     *mynoise2;

    int         noise_index;
    int         noise_index2;
    float       volume;
    float       attenuation;

    // timing variables
    float       wait;
    float       delay;          // before firing targets
    float       random;

    float       teleport_time;

    int         watertype;
    int         waterlevel;

    vec3_t      move_origin;
    vec3_t      move_angles;

    // move this to clientinfo?
    int         light_level;

    int         style;          // also used as areaportal number

    gitem_t     *item;          // for bonus items

    // common data blocks
    moveinfo_t      moveinfo;

    // hack for proper s.old_origin updates
    vec3_t      old_origin;
	
    int         arena;
    int         override;
    int         version;

    // list testing
    list_t      entry;

    edict_t     *killer;
};

//
// p_menu.c
//
void PMenu_Open(edict_t *ent, const pmenu_entry_t *entries);
void PMenu_Close(edict_t *ent);
void PMenu_Update(edict_t *ent);
void PMenu_Next(edict_t *ent);
void PMenu_Prev(edict_t *ent);
void PMenu_Select(edict_t *ent);

//
// g_chase.c
//
void ChaseEndServerFrame(edict_t *ent);
void ChaseNext(edict_t *ent);
void ChasePrev(edict_t *ent);
qboolean GetChaseTarget(edict_t *ent, chase_mode_t mode);
void SetChaseTarget(edict_t *ent, edict_t *targ);
void UpdateChaseTargets(chase_mode_t mode, edict_t *targ);

//
// g_vote.c
//
void G_FinishVote(void);
void G_UpdateVote(void);
qboolean G_CheckVote(void);
void Cmd_Vote_f(edict_t *ent);
void Cmd_CastVote_f(edict_t *ent, qboolean accepted);
uint8_t damage_vote_index(const char *name);
uint8_t weapon_vote_index(const char *name);

//
// g_bans.c
//

typedef enum {
    IPA_NONE,
    IPA_BAN,
    IPA_MUTE
} ipaction_t;

ipaction_t G_CheckFilters(char *s);
void G_AddIP_f(edict_t *ent);
void G_BanEdict(edict_t *victim, edict_t *initiator);
void G_RemoveIP_f(edict_t *ent);
void G_ListIP_f(edict_t *ent);
void G_WriteIP_f(void);

//
// g_sqlite.c
//
#if USE_SQLITE
void G_BeginLogging(void);
void G_EndLogging(void);
void G_LogClient(gclient_t *c);
void G_LogClients(void);
qboolean G_OpenDatabase(void);
void G_CloseDatabase(void);
#endif

#define WEAPON_MAX 	(10)
#define DAMAGE_MAX	(5)

typedef struct weaponvote_s {
    const char  *names[2];
    unsigned    value;		// arena weapon bitmask value
    int         itemindex;	// q2 item index value
    int         ammoindex;	// q2 item index value for ammo
} weaponinfo_t;


typedef struct damagevote_s {
	const char  *name;
	uint16_t    value;
} damagevote_t;

extern const weaponinfo_t   weaponvotes[WEAPON_MAX];
extern const damagevote_t   damagevotes[DAMAGE_MAX];

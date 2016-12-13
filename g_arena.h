
#define MAX_ARENAS				9
#define MAX_ARENA_TEAM_PLAYERS	10
#define ARENA_HOME_SKIN			"female/jezebel"
#define ARENA_AWAY_SKIN			"male/cypher"

#define str_equal(x, y)		(Q_stricmp((x), (y)) == 0)


typedef enum {
	ARENA_TEAM_NONE,
	ARENA_TEAM_HOME,
	ARENA_TEAM_AWAY,
} arena_team_type_t;



typedef enum {
	ARENA_STATE_WARMUP,
	ARENA_STATE_COUNTDOWN,
	ARENA_STATE_PLAY,
	ARENA_STATE_OVERTIME,
	ARENA_STATE_TIMEOUT,
	ARENA_STATE_ROUNDPAUSE,		// the gap between rounds in a match
} arena_state_t;



typedef struct {
	char 				name[20];
	char				skin[25];
	edict_t				*players[MAX_ARENA_TEAM_PLAYERS];
	edict_t				*captain;
	arena_team_type_t 	type;
	int					player_count;
	qboolean			locked;
} arena_team_t;



typedef struct {
	int				number;
	char			name[50];
	arena_state_t	state;
	int				player_count;
	int				spectator_count;
	arena_team_t	team_home;
	arena_team_t	team_away;
} arena_t;



// maps contain multiple arenas
typedef struct {
	char		name[50];
	uint8_t		arena;
	uint32_t	weapon_flags;
	uint32_t	damage_flags;
	uint8_t 	rounds;
	uint32_t 	round_timelimit;
} arena_entry_t;



void change_arena(edict_t *self);
void G_bprintf(arena_t *arena, int level, const char *fmt, ...);
qboolean G_CheckReady(arena_t *a);
void G_GiveItems(edict_t *ent);
void G_JoinTeam(edict_t *ent, arena_team_type_t type);
void G_PartTeam(edict_t *ent);
void G_SetSkin(edict_t *ent, const char *skin);
void G_StartingWeapon(edict_t *ent, int gun);

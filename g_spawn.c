
/*
Copyright (C) 1997-2001 Id Software, Inc.

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

typedef struct {
    char    *name;
    void    (*spawn)(edict_t *ent);
} spawn_t;

/**
 * Fields are needed for spawning from the entity string and saving / loading
 * games
 */
typedef enum {
    F_INT,
    F_FLOAT,
    F_LSTRING,  // string on disk, pointer in memory, TAG_LEVEL
    F_GSTRING,  // string on disk, pointer in memory, TAG_GAME
    F_VECTOR,
    F_ANGLEHACK,
    F_IGNORE
} fieldtype_t;

typedef struct {
    char        *name;
    size_t      ofs;
    fieldtype_t type;
} field_t;

void SP_item_health(edict_t *self);
void SP_item_health_small(edict_t *self);
void SP_item_health_large(edict_t *self);
void SP_item_health_mega(edict_t *self);

void SP_info_player_start(edict_t *ent);
void SP_info_player_deathmatch(edict_t *ent);
void SP_info_player_coop(edict_t *ent);
void SP_info_player_intermission(edict_t *ent);

void SP_func_plat(edict_t *ent);
void SP_func_rotating(edict_t *ent);
void SP_func_button(edict_t *ent);
void SP_func_door(edict_t *ent);
void SP_func_door_secret(edict_t *ent);
void SP_func_door_rotating(edict_t *ent);
void SP_func_water(edict_t *ent);
void SP_func_train(edict_t *ent);
void SP_func_conveyor(edict_t *self);
void SP_func_wall(edict_t *self);
void SP_func_object(edict_t *self);
void SP_func_timer(edict_t *self);
void SP_func_areaportal(edict_t *ent);
void SP_func_clock(edict_t *ent);
void SP_func_killbox(edict_t *ent);

void SP_trigger_always(edict_t *ent);
void SP_trigger_once(edict_t *ent);
void SP_trigger_multiple(edict_t *ent);
void SP_trigger_relay(edict_t *ent);
void SP_trigger_push(edict_t *ent);
void SP_trigger_hurt(edict_t *ent);
void SP_trigger_key(edict_t *ent);
void SP_trigger_counter(edict_t *ent);
void SP_trigger_elevator(edict_t *ent);
void SP_trigger_gravity(edict_t *ent);

void SP_target_temp_entity(edict_t *ent);
void SP_target_speaker(edict_t *ent);
void SP_target_explosion(edict_t *ent);
void SP_target_changelevel(edict_t *ent);
void SP_target_splash(edict_t *ent);
void SP_target_spawner(edict_t *ent);
void SP_target_blaster(edict_t *ent);
void SP_target_crosslevel_trigger(edict_t *ent);
void SP_target_crosslevel_target(edict_t *ent);
void SP_target_laser(edict_t *self);
void SP_target_earthquake(edict_t *ent);
void SP_target_character(edict_t *ent);
void SP_target_string(edict_t *ent);

void SP_worldspawn(edict_t *ent);
void SP_viewthing(edict_t *ent);

void SP_light_mine1(edict_t *ent);
void SP_light_mine2(edict_t *ent);
void SP_info_null(edict_t *self);
void SP_info_notnull(edict_t *self);
void SP_path_corner(edict_t *self);

void SP_misc_banner(edict_t *self);
void SP_misc_satellite_dish(edict_t *self);
void SP_misc_gib_arm(edict_t *self);
void SP_misc_gib_leg(edict_t *self);
void SP_misc_gib_head(edict_t *self);
void SP_misc_viper(edict_t *self);
void SP_misc_viper_bomb(edict_t *self);
void SP_misc_bigviper(edict_t *self);
void SP_misc_strogg_ship(edict_t *self);
void SP_misc_teleporter(edict_t *self);
void SP_misc_teleporter_dest(edict_t *self);
void SP_misc_blackhole(edict_t *self);
void SP_misc_eastertank(edict_t *self);
void SP_misc_easterchick(edict_t *self);
void SP_misc_easterchick2(edict_t *self);

void SP_monster_commander_body(edict_t *self);

static const spawn_t    g_spawns[] = {
    {"item_health", SP_item_health},
    {"item_health_small", SP_item_health_small},
    {"item_health_large", SP_item_health_large},
    {"item_health_mega", SP_item_health_mega},

    {"info_player_start", SP_info_player_start},
    {"info_player_deathmatch", SP_info_player_deathmatch},
    {"info_player_coop", SP_info_player_coop},
    {"info_player_intermission", SP_info_player_intermission},

    {"func_plat", SP_func_plat},
    {"func_button", SP_func_button},
    {"func_door", SP_func_door},
    {"func_door_secret", SP_func_door_secret},
    {"func_door_rotating", SP_func_door_rotating},
    {"func_rotating", SP_func_rotating},
    {"func_train", SP_func_train},
    {"func_water", SP_func_water},
    {"func_conveyor", SP_func_conveyor},
    {"func_areaportal", SP_func_areaportal},
    {"func_clock", SP_func_clock},
    {"func_wall", SP_func_wall},
    {"func_object", SP_func_object},
    {"func_timer", SP_func_timer},
    {"func_killbox", SP_func_killbox},

    {"trigger_always", SP_trigger_always},
    {"trigger_once", SP_trigger_once},
    {"trigger_multiple", SP_trigger_multiple},
    {"trigger_relay", SP_trigger_relay},
    {"trigger_push", SP_trigger_push},
    {"trigger_hurt", SP_trigger_hurt},
    {"trigger_key", SP_trigger_key},
    {"trigger_counter", SP_trigger_counter},
    {"trigger_elevator", SP_trigger_elevator},
    {"trigger_gravity", SP_trigger_gravity},

    {"target_temp_entity", SP_target_temp_entity},
    {"target_speaker", SP_target_speaker},
    {"target_explosion", SP_target_explosion},
    {"target_changelevel", SP_target_changelevel},
    {"target_splash", SP_target_splash},
    {"target_spawner", SP_target_spawner},
    {"target_blaster", SP_target_blaster},
    {"target_crosslevel_trigger", SP_target_crosslevel_trigger},
    {"target_crosslevel_target", SP_target_crosslevel_target},
    {"target_laser", SP_target_laser},
    {"target_earthquake", SP_target_earthquake},
    {"target_character", SP_target_character},
    {"target_string", SP_target_string},

    {"worldspawn", SP_worldspawn},
    {"viewthing", SP_viewthing},

    {"light_mine1", SP_light_mine1},
    {"light_mine2", SP_light_mine2},
    {"info_null", SP_info_null},
    {"func_group", SP_info_null},
    {"info_notnull", SP_info_notnull},
    {"path_corner", SP_path_corner},

    {"misc_banner", SP_misc_banner},
    {"misc_satellite_dish", SP_misc_satellite_dish},
    {"misc_gib_arm", SP_misc_gib_arm},
    {"misc_gib_leg", SP_misc_gib_leg},
    {"misc_gib_head", SP_misc_gib_head},
    {"misc_viper", SP_misc_viper},
    {"misc_viper_bomb", SP_misc_viper_bomb},
    {"misc_bigviper", SP_misc_bigviper},
    {"misc_strogg_ship", SP_misc_strogg_ship},
    {"misc_teleporter", SP_misc_teleporter},
    {"misc_teleporter_dest", SP_misc_teleporter_dest},
    {"misc_blackhole", SP_misc_blackhole},
    {"misc_eastertank", SP_misc_eastertank},
    {"misc_easterchick", SP_misc_easterchick},
    {"misc_easterchick2", SP_misc_easterchick2},

    {"monster_commander_body", SP_monster_commander_body},

    {NULL, NULL}
};

static const field_t g_fields[] = {
    {"classname", FOFS(classname), F_LSTRING},
    {"model", FOFS(model), F_LSTRING},
    {"spawnflags", FOFS(spawnflags), F_INT},
    {"speed", FOFS(speed), F_FLOAT},
    {"accel", FOFS(accel), F_FLOAT},
    {"decel", FOFS(decel), F_FLOAT},
    {"target", FOFS(target), F_LSTRING},
    {"targetname", FOFS(targetname), F_LSTRING},
    {"pathtarget", FOFS(pathtarget), F_LSTRING},
    {"deathtarget", FOFS(deathtarget), F_LSTRING},
    {"killtarget", FOFS(killtarget), F_LSTRING},
    {"combattarget", FOFS(combattarget), F_LSTRING},
    {"message", FOFS(message), F_LSTRING},
    {"team", FOFS(team), F_LSTRING},
    {"wait", FOFS(wait), F_FLOAT},
    {"delay", FOFS(delay), F_FLOAT},
    {"random", FOFS(random), F_FLOAT},
    {"move_origin", FOFS(move_origin), F_VECTOR},
    {"move_angles", FOFS(move_angles), F_VECTOR},
    {"style", FOFS(style), F_INT},
    {"count", FOFS(count), F_INT},
    {"health", FOFS(health), F_INT},
    {"sounds", FOFS(sounds), F_INT},
    {"light", 0, F_IGNORE},
    {"dmg", FOFS(dmg), F_INT},
    {"mass", FOFS(mass), F_INT},
    {"volume", FOFS(volume), F_FLOAT},
    {"attenuation", FOFS(attenuation), F_FLOAT},
    {"map", FOFS(map), F_LSTRING},
    {"origin", FOFS(s.origin), F_VECTOR},
    {"angles", FOFS(s.angles), F_VECTOR},
    {"angle", FOFS(s.angles), F_ANGLEHACK},
	{"arena", FOFS(arena), F_INT},
	{"override", FOFS(override), F_INT},    // for adding ents that would otherwise be filtered out
	{"version", FOFS(version), F_INT},      // map version (original ra2 and non-ra2 maps will be null)
    {NULL}
};

// temp spawn vars -- only valid when the spawn function is called
static const field_t g_temps[] = {
    {"lip", STOFS(lip), F_INT},
    {"distance", STOFS(distance), F_INT},
    {"height", STOFS(height), F_INT},
    {"noise", STOFS(noise), F_LSTRING},
    {"pausetime", STOFS(pausetime), F_FLOAT},
    {"item", STOFS(item), F_LSTRING},
    {"gravity", STOFS(gravity), F_LSTRING},
    {"sky", STOFS(sky), F_LSTRING},
    {"skyrotate", STOFS(skyrotate), F_FLOAT},
    {"skyaxis", STOFS(skyaxis), F_VECTOR},
    {"minyaw", STOFS(minyaw), F_FLOAT},
    {"maxyaw", STOFS(maxyaw), F_FLOAT},
    {"minpitch", STOFS(minpitch), F_FLOAT},
    {"maxpitch", STOFS(maxpitch), F_FLOAT},
    {"nextmap", STOFS(nextmap), F_LSTRING},
    {NULL}
};

/**
 * Parse strings such as:
 *   "-all +rg +cg:600 +gl"
 *   "+all -bfg"
 *   "-all +hg:inf"
 *
 * Used in voting and such
 */
qboolean G_ParseWeaponString(arena_t *arena, edict_t *ent, const char **input, temp_weaponflags_t *t) {
    char *token;
    const char *fulltoken;
    qboolean modifier;
    char **weapammopair;
    weaponinfo_t w;
    int8_t windex;
    long testval;
    char *next;

    uint16_t defaultammo[MAX_INVENTORY];
    defaultammo[ITEM_SHELLS] = 20;
    defaultammo[ITEM_BULLETS] = 200;
    defaultammo[ITEM_GRENADES] = 10;
    defaultammo[ITEM_CELLS] = 100;
    defaultammo[ITEM_ROCKETS] = 15;
    defaultammo[ITEM_SLUGS] = 5;

    if (arena) {
        t->weaponflags = arena->weapon_flags;
        memcpy(t->ammo, arena->ammo, sizeof(t->ammo));
        memcpy(t->infinite, arena->infinite, sizeof(t->infinite));
    }

    fulltoken = COM_Parse_Newline(input);

    // check if it's a number, if so assume raw dmg flags value
    testval = strtol(fulltoken, &next, 10);
    if (!((next == fulltoken) || (*next != '\0'))) {
        t->weaponflags = testval;
        memcpy(t->ammo, defaultammo, sizeof(t->ammo));
        memset(t->infinite, 0, sizeof(t->infinite));
        return qtrue;
    }

    token = COM_Parse(&fulltoken);

    while (token[0]) {
        // parse out the +/- modifier
        if (token[0] == '-') {
            modifier = qfalse;
            token++;
        } else if (token[0] == '+') {
            modifier = qtrue;
            token++;
        } else { // no modifier, assume default to add
            modifier = qtrue;
        }

        // only really for voting
        if (str_equal(token, "reset")) {
            t->weaponflags = arena->original_weapon_flags;
            memcpy(t->ammo, arena->defaultammo, sizeof(t->ammo));
            memcpy(t->infinite, arena->defaultinfinite, sizeof(t->infinite));
            return qtrue;
        }

        if (str_equal(token, "random")) {
            t->weaponflags = genrand_int32() & WEAPONFLAG_MASK;
            G_RandomizeAmmo(t->ammo);
            return qtrue;
        }

        if (str_equal(token, "all")) {
            t->weaponflags = (modifier) ? ARENAWEAPON_ALL : 0;
            if (modifier) {
                if (arena) {
                    memcpy(t->ammo, arena->defaultammo, sizeof(t->ammo));
                } else {
                    memcpy(t->ammo, defaultammo, sizeof(t->ammo));
                }
                memset(t->infinite, 0, sizeof(t->infinite));
            }
            token = COM_Parse(&fulltoken);
            continue;
        }

        // ammo specified
        if (strstr(token, ":")) {
            weapammopair = KeyValueSplit(token, ":", 100);
            windex = weapon_vote_index(weapammopair[0]);

            if (windex > -1) {
                w = weaponvotes[windex];
                if (modifier) {
                    t->weaponflags |= w.value;
                    if (str_equal(weapammopair[1], "inf")) {
                        t->ammo[w.ammoindex] = 666;
                        t->infinite[w.ammoindex] = qtrue;
                    } else {
                        t->ammo[w.ammoindex] = strtoul(weapammopair[1], NULL, 10);
                        clamp(t->ammo[w.ammoindex], 1, 999);
                        t->infinite[w.ammoindex] = qfalse;
                    }
                } else {
                    t->weaponflags &= ~w.value;
                    t->ammo[w.ammoindex] = 0;
                    t->infinite[w.ammoindex] = qfalse;
                }
            } else {
                gi.cprintf(ent, PRINT_HIGH, "Unknown weapon '%s'\n", weapammopair[0]);
                // free weapammopair?
                return qfalse;
            }
            // free weapammopair?
        } else { // just gun, use default for ammo
            windex = weapon_vote_index(token);
            if (windex > -1) {
                w = weaponvotes[windex];
                if (modifier) {
                    t->weaponflags |= w.value;
                    if (arena) {
                        t->ammo[w.ammoindex] = arena->defaultammo[w.ammoindex];
                    } else {
                        t->ammo[w.ammoindex] = defaultammo[w.ammoindex];
                    }
                    clamp(t->ammo[w.ammoindex], 1, 999);
                } else {
                    t->weaponflags &= ~w.value;
                    // dont take ammo away, in case shared (sg/ssg, mg/cg)
                }
            } else {
                gi.cprintf(ent, PRINT_HIGH, "Unknown weapon '%s'\n", token);
                return qfalse;
            }
        }
        token = COM_Parse(&fulltoken);
    }
    return qtrue;
}

/**
 * Take a string like "-all +falling +self" and translate that to a integer bitmask
 */
qboolean G_ParseDamageString(arena_t *a, edict_t *ent, const char **input, uint32_t *target) {
    qboolean modifier;
    const char *fulltoken;
    char *token;
    uint8_t index;
    long testval;
    char *next;

    fulltoken = COM_Parse_Newline(input);

    // check if it's a number, if so assume raw dmg flags value
    testval = strtol(fulltoken, &next, 10);
    if (!((next == fulltoken) || (*next != '\0'))) {
        *target = testval;
        return qtrue;
    }

    token = COM_Parse(&fulltoken);
    while (token[0]) {
        // EOL
        if (token[0] == '\n') {
            return qtrue;
        }

        // parse out the +/- modifier
        if (token[0] == '-') {
            modifier = qfalse;
            token++;
        } else if (token[0] == '+') {
            modifier = qtrue;
            token++;
        } else { // no modifier, assume add
            modifier = qtrue;
        }

        // reset back to original status
        if (str_equal(token, "reset")) {
            *target = (a) ? a->original_damage_flags : 0;
            return qtrue;
        }

        if (str_equal(token, "all")) {
            *target = (modifier) ? ARENADAMAGE_ALL : 0;
            token = COM_Parse(&fulltoken);
            continue;
        }

        index = damage_vote_index(token);
        if (index > -1) {
            if (modifier) {
                *target |= damagevotes[index].value;
            } else {
                *target &= ~damagevotes[index].value;
            }
        } else {
            gi.cprintf(ent, PRINT_HIGH, "Unknown damage type '%s'", token);
            return qfalse;
        }
        token = COM_Parse(&fulltoken);
    }
    return qtrue;
}

/**
 * Finds the spawn function for the entity and calls it
 */
void ED_CallSpawn(edict_t *ent) {
    const spawn_t   *s;
    const gitem_t   *item;
    int     i;

    if (!ent->classname) {
        gi.dprintf("%s: NULL classname\n", __func__);
        return;
    }

    // check item spawn functions
    for (i = 0, item = g_itemlist; i < ITEM_TOTAL; i++, item++) {
        if (!item->classname) {
            continue;
        }
        if (!strcmp(item->classname, ent->classname)) { // found it
            SpawnItem(ent, (gitem_t *)item);
            return;
        }
    }

    // check normal spawn functions
    for (s = g_spawns; s->name; s++) {
        if (!strcmp(s->name, ent->classname)) { // found it
            s->spawn(ent);
            return;
        }
    }
    G_FreeEdict(ent);
}

/**
 *
 */
static char *ED_NewString(const char *string) {
    char    *newb, *new_p;
    int     i, l;

    l = strlen(string) + 1;
    newb = gi.TagMalloc(l, TAG_LEVEL);
    new_p = newb;
    for (i = 0; i < l; i++) {
        if (string[i] == '\\' && i < l - 1) {
            i++;
            if (string[i] == 'n') {
                *new_p++ = '\n';
            } else {
                *new_p++ = '\\';
            }
        } else {
            *new_p++ = string[i];
        }
    }
    return newb;
}

/**
 * Takes a key/value pair and sets the binary values in an edict
 */
static qboolean ED_ParseField(const field_t *fields, const char *key, const char *value, byte *b) {
    const field_t   *f;
    float   v;
    vec3_t  vec;

    for (f = fields; f->name; f++) {
        if (!Q_stricmp(f->name, key)) {
            // found it
            switch (f->type) {
            case F_LSTRING:
                *(char **)(b + f->ofs) = ED_NewString(value);
                break;
            case F_VECTOR:
                if (sscanf(value, "%f %f %f", &vec[0], &vec[1], &vec[2]) != 3) {
                    gi.dprintf("%s: couldn't parse '%s'\n", __func__, key);
                    VectorClear(vec);
                }
                ((float *)(b + f->ofs))[0] = vec[0];
                ((float *)(b + f->ofs))[1] = vec[1];
                ((float *)(b + f->ofs))[2] = vec[2];
                break;
            case F_INT:
                *(int *)(b + f->ofs) = atoi(value);
                break;
            case F_FLOAT:
                *(float *)(b + f->ofs) = atof(value);
                break;
            case F_ANGLEHACK:
                v = atof(value);
                ((float *)(b + f->ofs))[0] = 0;
                ((float *)(b + f->ofs))[1] = v;
                ((float *)(b + f->ofs))[2] = 0;
                break;
            case F_IGNORE:
                break;
            default:
                break;
            }
            return qtrue;
        }
    }
    return qfalse;
}

/**
 * Parses an edict out of the given string, returning the new position.
 *
 * ed should be a properly initialized empty edict.
 */
static void ED_ParseEdict(const char **data, edict_t *ent) {
    qboolean    init;
    char        *key, *value;

    init = qfalse;
    memset(&st, 0, sizeof(st));

    // go through all the dictionary pairs
    while (1) {
        // parse key
        key = COM_Parse(data);
        if (key[0] == '}') {
            break;
        }
        if (!*data) {
            gi.error("%s: EOF without closing brace", __func__);
        }

        // parse value
        value = COM_Parse(data);
        if (!*data) {
            gi.error("%s: EOF without closing brace", __func__);
        }

        if (value[0] == '}') {
            gi.error("%s: closing brace without data", __func__);
        }

        init = qtrue;

        // keynames with a leading underscore are used for utility comments,
        // and are immediately discarded by quake
        if (key[0] == '_') {
            continue;
        }

        if (!ED_ParseField(g_fields, key, value, (byte *)ent)) {
            if (!ED_ParseField(g_temps, key, value, (byte *)&st)) {
                gi.dprintf("%s: %s is not a field\n", __func__, key);
            }
        }
    }

    if (!init) {
        memset(ent, 0, sizeof(*ent));
    }
}

/**
 * Chain together all entities with a matching team field.
 *
 * All but the first will have the FL_TEAMSLAVE flag set.
 * All but the last will have the teamchain field set to the next one
 */
static void G_FindTeams(void) {
    edict_t *e, *e2, *chain;
    int     i, j;
    int     c, c2;

    c = 0;
    c2 = 0;
    for (i = 1, e = g_edicts + i; i < globals.num_edicts; i++, e++) {
        if (!e->inuse) {
            continue;
        }
        if (!e->team) {
            continue;
        }
        if (e->flags & FL_TEAMSLAVE) {
            continue;
        }
        chain = e;
        e->teammaster = e;
        c++;
        c2++;
        for (j = i + 1, e2 = e + 1; j < globals.num_edicts; j++, e2++) {
            if (!e2->inuse) {
                continue;
            }
            if (!e2->team) {
                continue;
            }
            if (e2->flags & FL_TEAMSLAVE) {
                continue;
            }
            if (!strcmp(e->team, e2->team)) {
                c2++;
                chain->teamchain = e2;
                e2->teammaster = e;
                chain = e2;
                e2->flags |= FL_TEAMSLAVE;
            }
        }
    }
    gi.cprintf(NULL, PRINT_HIGH, "%i teams with %i entities\n", c, c2);
}

/**
 *
 */
static void G_ParseString(void) {
    const char  *entities = level.entstring;
    edict_t     *ent;
    int         inhibit = 0;
    char        *token;

    // parse ents
    while (1) {
        // parse the opening brace
        token = COM_Parse(&entities);
        if (!entities) {
            break;
        }
        if (token[0] != '{') {
            gi.error("%s: found %s when expecting {", __func__, token);
        }

        ent = G_Spawn();
        ED_ParseEdict(&entities, ent);

        // remove things from different skill levels or deathmatch
        if (ent->spawnflags & SPAWNFLAG_NOT_DEATHMATCH) {
            G_FreeEdict(ent);
            inhibit++;
            continue;
        }

        // Inhibit most if not all entities from spawning
        if ((strstr(ent->classname, "ammo_") ||
            strstr(ent->classname, "weapon_") ||
            strstr(ent->classname, "item_")) && !ent->override) {
            G_FreeEdict(ent);
            inhibit++;
        }
        ent->spawnflags &= ~INHIBIT_MASK;
        ED_CallSpawn(ent);
    }
    gi.cprintf(NULL, PRINT_HIGH, "%i entities inhibited\n", inhibit);
}

/**
 * Initialize all teams. Set their types, names and skins
 */
void G_InitArenaTeams(arena_t *arena) {
    arena_team_t *team;
    uint8_t i = 0;

    // team 1
    team = &arena->teams[i++];
    team->type = TEAM_RED;
    Q_strlcpy(team->skin, g_team1_skin->string, sizeof(team->skin));
    Q_strlcpy(team->name, g_team1_name->string, sizeof(team->name));

    // team 2
    team = &arena->teams[i++];
    team->type = TEAM_BLUE;
    Q_strlcpy(team->skin, g_team2_skin->string, sizeof(team->skin));
    Q_strlcpy(team->name, g_team2_name->string, sizeof(team->name));

    // team 3
    if (arena->team_count >= i) {
        team = &arena->teams[i++];
        team->type = TEAM_GREEN;
        Q_strlcpy(team->skin, g_team3_skin->string, sizeof(team->skin));
        Q_strlcpy(team->name, g_team3_name->string, sizeof(team->name));
    }

    // team 4
    if (arena->team_count >= i) {
        team = &arena->teams[i++];
        team->type = TEAM_YELLOW;
        Q_strlcpy(team->skin, g_team4_skin->string, sizeof(team->skin));
        Q_strlcpy(team->name, g_team4_name->string, sizeof(team->name));
    }

    // team 5
    if (arena->team_count >= i) {
        team = &arena->teams[i++];
        team->type = TEAM_AQUA;
        Q_strlcpy(team->skin, g_team5_skin->string, sizeof(team->skin));
        Q_strlcpy(team->name, g_team5_name->string, sizeof(team->name));
    }
}

/**
 * Recursively sort the arenas array by their numbers.
 *
 * Called from G_SpawnEntities() after arenas are parsed from the .bsp map
 */
void sort_arenas() {
    qboolean swapped = false;
    arena_t temp;

    for (int i=0; i<level.arena_count-1; i++) {
        if (level.arenas[i].number > level.arenas[i+1].number) {
            temp = level.arenas[i];
            level.arenas[i] = level.arenas[i+1];
            level.arenas[i+1] = temp;
            swapped = qtrue;
        }
    }

    if (swapped) {
        sort_arenas();
    }
}

/**
 * Creates a server's entity / program execution context by parsing textual
 * entity definitions out of an ent file.
 */
void G_SpawnEntities(const char *mapname, const char *entities, const char *spawnpoint) {
    edict_t     *ent;
    gclient_t   *client;
    int         i, j;
    client_persistant_t pers;
    char        *token;
    char        playerskin[MAX_QPATH];
    qboolean    notra2map = qfalse;
    size_t      parsed_arenas;

#if USE_SQLITE
    G_OpenDatabase();
    G_LogClients();
#endif

    gi.FreeTags(TAG_LEVEL);

    memset(&level, 0, sizeof(level));
    memset(g_edicts, 0, game.maxentities * sizeof(g_edicts[0]));

    Q_strlcpy(level.mapname, mapname, sizeof(level.mapname));

    // set client fields on player ents
    for (i = 0; i < game.maxclients; i++) {
        ent = &g_edicts[i + 1];
        client = &game.clients[i];
        ent->client = client;
        ent->inuse = qfalse;

        if (!client->pers.connected) {
            continue;
        }

        // clear everything but the persistent data
        pers = client->pers;
        memset(client, 0, sizeof(*client));
        client->pers = pers;
        client->edict = ent;
        client->clientNum = i;
        client->pers.connected = CONN_CONNECTED;
        client->pers.arena = NULL;
        client->pers.team = NULL;

        // combine name and skin into a configstring
        Q_concat(playerskin, sizeof(playerskin),
                 client->pers.netname, "\\", client->pers.skin, NULL);
        gi.configstring(CS_PLAYERSKINS + i, playerskin);
        gi.configstring(CS_PLAYERNAMES + i, client->pers.netname);
    }

    // parse worldspawn
    token = COM_Parse(&entities);
    if (!entities) {
        gi.error("%s: empty entity string", __func__);
    }
    if (token[0] != '{') {
        gi.error("%s: found %s when expecting {", __func__, token);
    }

    ent = g_edicts;
    ED_ParseEdict(&entities, ent);
    ED_CallSpawn(ent);

    level.entstring = entities;

    G_ParseString();
    G_FindTeams();	// teamed entities not player teams

    memset(&level.arena_defaults, 0, sizeof(arena_entry_t) * MAX_ARENAS);
    parsed_arenas = G_ParseMapSettings(level.arena_defaults, mapname);

    // find arenas
    List_Init(&g_arenalist);
    ent = NULL;
    while ((ent = G_Find(ent, FOFS(classname), "info_player_intermission")) != NULL) {
        if (!ent->arena || ent->arena >= MAX_ARENAS) {
            continue;
        }

        j = ent->arena;

        memset(&level.arenas[j], 0, sizeof(arena_t));

        level.arenas[j].number = ent->arena;
        level.arenas[j].round_limit = (int) g_round_limit->value;
        level.arenas[j].weapon_flags = (int) g_weapon_flags->value;
        level.arenas[j].damage_flags = (int) g_damage_flags->value;
        level.arenas[j].armor = (int) g_armor_start->value;
        level.arenas[j].health = (int) g_health_start->value;
        level.arenas[j].team_count = (int) g_team_count->value;
        level.arenas[j].timelimit = (int) g_round_timelimit->value;
        level.arenas[j].fastswitch = (int) g_fast_weapon_change->value;
        level.arenas[j].mode = ARENA_MODE_NORMAL;
        level.arenas[j].corpseview = qfalse;
        level.arenas[j].scoremode = (int) g_scoremode->value;

        Q_strlcpy(level.arenas[j].name, ent->message, sizeof(level.arenas[j].name));

        // apply overrides for default flags
        G_MergeArenaSettings(&level.arenas[j], &level.arena_defaults[j]);

        // setup the teams
        G_InitArenaTeams(&level.arenas[j]);

        level.arena_count++;
    }

    sort_arenas();
    for (int i=1; i<=level.arena_count; i++) {
        List_Append(&g_arenalist, &level.arenas[i].entry);
    }

    // find the default arena
    ent = NULL;
    while ((ent = G_Find(ent, FOFS(classname), "worldspawn")) != NULL) {
        if (ent->arena) {
            level.default_arena = ent->arena;
            break;
        }
    }

    // not an ra2 map, make the map arena #1
    if (level.arena_count == 0) {
        j = 1;
        notra2map = qtrue;

        gi.dprintf("%s is not a native arena map, emulating a single arena...\n", mapname);
        level.arenas[j].number = j;
        Q_strlcpy(level.arenas[j].name, mapname, sizeof(level.arenas[j].name));

        if (parsed_arenas > 0) {
            G_MergeArenaSettings(&level.arenas[j], &level.arena_defaults[j]);
        } else {
            G_MergeArenaSettings(&level.arenas[j], NULL);
        }

        G_InitArenaTeams(&(level.arenas[j]));
        level.arena_count++;
        List_Append(&g_arenalist, &level.arenas[j].entry);
    }

    G_BuildMenu();

    level.map = G_FindMap(mapname);

    // find spawnpoints
    ent = NULL;
    while ((ent = G_Find(ent, FOFS(classname), "info_player_deathmatch")) != NULL) {
        if (notra2map) {
            ent->arena = 1;
        }

        // hide it
        ent->flags |= FL_HIDDEN;
        ent->svflags |= SVF_NOCLIENT;
        ent->solid = SOLID_NOT;

        level.spawns[level.numspawns++] = ent;
        if (level.numspawns == MAX_SPAWNS) {
            break;
        }
    }

    gi.dprintf("%d spawn points\n", level.numspawns);
    gi.dprintf("%d arena%s\n", level.arena_count, (level.arena_count > 1) ? "s":"");

    if (DEBUG) {
        arena_t *a;
        FOR_EACH_ARENA(a) {
            gi.dprintf("  [%d] %s\n", a->number, a->name);
        }
    }

    if ((int)timelimit->value > 0) {
        ClockStartMapTimelimit((int)timelimit->value * 60);
    }
}

/**
 *
 */
void G_ResetLevel(void) {
    gclient_t *client;
    edict_t *ent;
    int i;

    gi.FreeTags(TAG_LEVEL);

#if USE_SQLITE
    G_LogClients();
#endif

    G_FinishVote();

    // clear level
    level.framenum = 0;
    level.time = 0;
    level.intermission_framenum = 0;
    level.intermission_exit = 0;
    level.vote.proposal = 0;
    level.nextmap[0] = 0;
    level.record = 0;
    level.players_in = level.players_out = 0;

    // free all edicts
    for (i = game.maxclients + 1; i < globals.num_edicts; i++) {
        ent = &g_edicts[i];
        if (ent->inuse) {
            G_FreeEdict(ent);
        }
    }
    globals.num_edicts = game.maxclients + 1;

    InitBodyQue();

    // respawn all edicts
    G_ParseString();
    G_FindTeams();

    // respawn all clients
    for (i = 0; i < game.maxclients; i++) {
        client = &game.clients[i];
        if (!client->pers.connected) {
            continue;
        }
        memset(&client->resp, 0, sizeof(client->resp));
        memset(&client->level.vote, 0, sizeof(client->level.vote));
        if (client->pers.connected == CONN_SPAWNED) {
            ent = client->edict;
            G_ScoreChanged(ent);
            ent->movetype = MOVETYPE_NOCLIP; // do not leave body
            G_RespawnPlayer(ent);
        }
    }

    G_UpdateRanks();

    if (timelimit->value > 0) {
        int remaining = timelimit->value * 60 - level.time;

        G_WriteTime(remaining);
        gi.multicast(NULL, MULTICAST_ALL);
    }

    // allow everything to settle
    G_RunFrame();
    G_RunFrame();

    // make sure movers are not interpolated
    for (i = game.maxclients + 1; i < globals.num_edicts; i++) {
        ent = &g_edicts[i];
        if (ent->inuse && ent->movetype) {
            ent->s.event = EV_OTHER_TELEPORT;
        }
    }
}

static const char statusbar[] =
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
	"yb -76 "
	"xv 0 "
	"stat_string 31 "
"endif "
;

/**
 * Only used for the world.
 *
 * "sky"   environment map name
 * "skyaxis"   vector axis for rotating sky
 * "skyrotate" speed of rotation in degrees/second
 * "sounds"    music cd track number
 * "gravity"   800 is default gravity
 * "message"   text to print at user logon
 */
void SP_worldspawn(edict_t *ent) {
    char buffer[MAX_QPATH];

    ent->movetype = MOVETYPE_PUSH;
    ent->solid = SOLID_BSP;
    ent->inuse = qtrue;         // since the world doesn't use G_Spawn()
    ent->s.modelindex = 1;      // world model is always index 1

    InitBodyQue();  // reserve some spots for dead player bodies
    SetItemNames(); // set configstrings for items

    if (ent->message && ent->message[0]) {
        gi.configstring(CS_NAME, ent->message);
    }
    ent->message = NULL;
    gi.configstring(CS_SKY, (st.sky && st.sky[0]) ? st.sky : "unit1_");
    gi.configstring(CS_SKYROTATE, va("%f", st.skyrotate));
    gi.configstring(CS_SKYAXIS, va("%f %f %f", st.skyaxis[0], st.skyaxis[1], st.skyaxis[2]));
    gi.configstring(CS_CDTRACK, va("%i", ent->sounds));
    gi.configstring(CS_MAXCLIENTS, va("%i", (int)(maxclients->value)));
    gi.configstring(CS_STATUSBAR, statusbar);
    gi.configstring(CS_OBSERVE, "SPECT");
    G_HighlightStr(buffer, "SPECTATOR MODE", sizeof(buffer));
    gi.configstring(CS_SPECMODE, buffer);

    gi.imageindex("i_help");
    level.images.health = gi.imageindex("i_health");
    gi.imageindex("help");
    gi.imageindex("field_3");

    level.images.powershield = gi.imageindex("i_powershield");
    level.images.quad = gi.imageindex("p_quad");
    level.images.invulnerability = gi.imageindex("p_invulnerability");
    level.images.envirosuit = gi.imageindex("p_envirosuit");
    level.images.rebreather = gi.imageindex("p_rebreather");

    gi.cvar_set("sv_gravity", (!st.gravity) ? "800" : st.gravity);

    level.sounds.fry = gi.soundindex("player/fry.wav"); // standing in lava / slime
    level.sounds.lava_in = gi.soundindex("player/lava_in.wav");
    level.sounds.burn[0] = gi.soundindex("player/burn1.wav");
    level.sounds.burn[1] = gi.soundindex("player/burn2.wav");
    level.sounds.drown = gi.soundindex("player/drown1.wav");

    PrecacheItem(INDEX_ITEM(ITEM_BLASTER));

    gi.soundindex("player/lava1.wav");
    gi.soundindex("player/lava2.wav");
    gi.soundindex("misc/pc_up.wav");
    gi.soundindex("misc/talk1.wav");
    gi.soundindex("items/respawn1.wav");

    level.sounds.secret = gi.soundindex("misc/secret.wav");
    level.sounds.count = gi.soundindex("world/10_0.wav");
    level.sounds.teleport = gi.soundindex("misc/tele1.wav");
    level.sounds.udeath = gi.soundindex("misc/udeath.wav");

    level.sounds.fight[1] = gi.soundindex("fight1.wav");
    level.sounds.fight[2] = gi.soundindex("fight2.wav");
    level.sounds.fight[3] = gi.soundindex("fight3.wav");
    level.sounds.fight[4] = gi.soundindex("fight4.wav");
    level.sounds.fight[5] = gi.soundindex("fight5.wav");
    level.sounds.fight[6] = gi.soundindex("fight6.wav");

    level.sounds.countdown[1] = gi.soundindex("count01.wav");
    level.sounds.countdown[2] = gi.soundindex("count02.wav");
    level.sounds.countdown[3] = gi.soundindex("count03.wav");
    level.sounds.countdown[4] = gi.soundindex("count04.wav");
    level.sounds.countdown[5] = gi.soundindex("count05.wav");
    level.sounds.countdown[6] = gi.soundindex("count06.wav");
    level.sounds.countdown[7] = gi.soundindex("count07.wav");
    level.sounds.countdown[8] = gi.soundindex("count08.wav");
    level.sounds.countdown[9] = gi.soundindex("count09.wav");
    level.sounds.countdown[10] = gi.soundindex("count10.wav");
    level.sounds.countdown[11] = gi.soundindex("count11.wav");
    level.sounds.countdown[12] = gi.soundindex("count12.wav");

    level.sounds.round[1] = gi.soundindex("round01.wav");
    level.sounds.round[2] = gi.soundindex("round02.wav");
    level.sounds.round[3] = gi.soundindex("round03.wav");
    level.sounds.round[4] = gi.soundindex("round04.wav");
    level.sounds.round[5] = gi.soundindex("round05.wav");
    level.sounds.round[6] = gi.soundindex("round06.wav");
    level.sounds.round[7] = gi.soundindex("round07.wav");
    level.sounds.round[8] = gi.soundindex("round08.wav");
    level.sounds.round[9] = gi.soundindex("round09.wav");
    level.sounds.round[10] = gi.soundindex("round10.wav");
    level.sounds.round[11] = gi.soundindex("round11.wav");
    level.sounds.round[12] = gi.soundindex("round12.wav");
    level.sounds.round[13] = gi.soundindex("round13.wav");
    level.sounds.round[14] = gi.soundindex("round14.wav");
    level.sounds.round[15] = gi.soundindex("round15.wav");
    level.sounds.round[16] = gi.soundindex("round16.wav");
    level.sounds.round[17] = gi.soundindex("round17.wav");
    level.sounds.round[18] = gi.soundindex("round18.wav");
    level.sounds.round[19] = gi.soundindex("round19.wav");
    level.sounds.round[20] = gi.soundindex("round20.wav");
    level.sounds.round[21] = gi.soundindex("round21.wav");

    level.sounds.finalround = gi.soundindex("roundfinal.wav");
    level.sounds.timeout = gi.soundindex("timeout.wav");
    level.sounds.timein = gi.soundindex("timein.wav");
    level.sounds.winner = gi.soundindex("winner.wav");
    level.sounds.loser = gi.soundindex("loser.wav");
    level.sounds.yousuck = gi.soundindex("yousuck.wav");
    level.sounds.impressive = gi.soundindex("impressive.wav");
    level.sounds.flawless = gi.soundindex("flawless.wav");
    level.sounds.horn = gi.soundindex("horn2.wav");

    // sexed sounds
    level.sounds.death[0] = gi.soundindex("*death1.wav");
    level.sounds.death[1] = gi.soundindex("*death2.wav");
    level.sounds.death[2] = gi.soundindex("*death3.wav");
    level.sounds.death[3] = gi.soundindex("*death4.wav");
    level.sounds.fall[0] = gi.soundindex("*fall1.wav");
    level.sounds.fall[1] = gi.soundindex("*fall2.wav");
    level.sounds.gurp[0] = gi.soundindex("*gurp1.wav");     // drowning damage
    level.sounds.gurp[1] = gi.soundindex("*gurp2.wav");
    level.sounds.jump = gi.soundindex("*jump1.wav");        // player jump
    level.sounds.pain[0][0] = gi.soundindex("*pain25_1.wav");
    level.sounds.pain[0][1] = gi.soundindex("*pain25_2.wav");
    level.sounds.pain[1][0] = gi.soundindex("*pain50_1.wav");
    level.sounds.pain[1][1] = gi.soundindex("*pain50_2.wav");
    level.sounds.pain[2][0] = gi.soundindex("*pain75_1.wav");
    level.sounds.pain[2][1] = gi.soundindex("*pain75_2.wav");
    level.sounds.pain[3][0] = gi.soundindex("*pain100_1.wav");
    level.sounds.pain[3][1] = gi.soundindex("*pain100_2.wav");
    level.sounds.rg_hum = gi.soundindex("weapons/rg_hum.wav");
    level.sounds.bfg_hum = gi.soundindex("weapons/bfg_hum.wav");

    // sexed models
    // THIS ORDER MUST MATCH THE DEFINES IN g_local.h
    // you can add more, max 15
    gi.modelindex("#w_blaster.md2");
    gi.modelindex("#w_shotgun.md2");
    gi.modelindex("#w_sshotgun.md2");
    gi.modelindex("#w_machinegun.md2");
    gi.modelindex("#w_chaingun.md2");
    gi.modelindex("#a_grenades.md2");
    gi.modelindex("#w_glauncher.md2");
    gi.modelindex("#w_rlauncher.md2");
    gi.modelindex("#w_hyperblaster.md2");
    gi.modelindex("#w_railgun.md2");
    gi.modelindex("#w_bfg.md2");

    level.sounds.gasp[0] = gi.soundindex("player/gasp1.wav");       // gasping for air
    level.sounds.gasp[1] = gi.soundindex("player/gasp2.wav");       // head breaking surface, not gasping
    level.sounds.watr_in = gi.soundindex("player/watr_in.wav");     // feet hitting water
    level.sounds.watr_out = gi.soundindex("player/watr_out.wav");   // feet leaving water
    level.sounds.watr_un = gi.soundindex("player/watr_un.wav");     // head going underwater
    level.sounds.breath[0] = gi.soundindex("player/u_breath1.wav");
    level.sounds.breath[1] = gi.soundindex("player/u_breath2.wav");

    gi.soundindex("items/pkup.wav");        // bonus item pickup
    gi.soundindex("world/land.wav");        // landing thud
    gi.soundindex("misc/h2ohit1.wav");      // landing splash
    gi.soundindex("items/damage.wav");
    gi.soundindex("items/protect.wav");
    gi.soundindex("items/protect4.wav");
    gi.soundindex("infantry/inflies1.wav");

    level.sounds.noammo = gi.soundindex("weapons/noammo.wav");
    level.sounds.xian = gi.soundindex("world/xian1.wav");
    level.sounds.makron = gi.soundindex("makron/laf4.wav");

    // gibs, don't bother with bones, chest, skulls...
    level.models.meat = gi.modelindex("models/objects/gibs/sm_meat/tris.md2");
    level.models.head = gi.modelindex("models/objects/gibs/head2/tris.md2");

    /**
     * Setup light animation tables. 'a' is total darkness, 'z' is doublebright.
     */
    // 0 normal
    gi.configstring(CS_LIGHTS + 0, "m");

    // 1 FLICKER (first variety)
    gi.configstring(CS_LIGHTS + 1, "mmnmmommommnonmmonqnmmo");

    // 2 SLOW STRONG PULSE
    gi.configstring(CS_LIGHTS + 2, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba");

    // 3 CANDLE (first variety)
    gi.configstring(CS_LIGHTS + 3, "mmmmmaaaaammmmmaaaaaabcdefgabcdefg");

    // 4 FAST STROBE
    gi.configstring(CS_LIGHTS + 4, "mamamamamama");

    // 5 GENTLE PULSE 1
    gi.configstring(CS_LIGHTS + 5, "jklmnopqrstuvwxyzyxwvutsrqponmlkj");

    // 6 FLICKER (second variety)
    gi.configstring(CS_LIGHTS + 6, "nmonqnmomnmomomno");

    // 7 CANDLE (second variety)
    gi.configstring(CS_LIGHTS + 7, "mmmaaaabcdefgmmmmaaaammmaamm");

    // 8 CANDLE (third variety)
    gi.configstring(CS_LIGHTS + 8, "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa");

    // 9 SLOW STROBE (fourth variety)
    gi.configstring(CS_LIGHTS + 9, "aaaaaaaazzzzzzzz");

    // 10 FLUORESCENT FLICKER
    gi.configstring(CS_LIGHTS + 10, "mmamammmmammamamaaamammma");

    // 11 SLOW PULSE NOT FADE TO BLACK
    gi.configstring(CS_LIGHTS + 11, "abcdefghijklmnopqrrqponmlkjihgfedcba");

    // styles 32-62 are assigned by the light program for switchable lights

    // 63 testing
    gi.configstring(CS_LIGHTS + 63, "a");

    char ready[6];
    G_AsciiToConsole(ready, "ready");
    gi.configstring(CS_READY, va("Type %s in console to begin", ready));
    gi.configstring(CS_READY_WAIT, "Waiting for all players to ready-up");
    gi.configstring(CS_READY_BALANCED, "Teams need to be balanced before starting");
}

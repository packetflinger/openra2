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

#include "g_local.h"

LIST_DECL(g_arenalist);

game_locals_t  game;
level_locals_t level;
game_import_t  gi;
game_export_t  globals;
spawn_temp_t   st;

int meansOfDeath;

edict_t *g_edicts;
edict_t *g_spectators[MAX_ARENAS];

pmenu_arena_t menu_lookup[MAX_ARENAS];

cvar_t *dmflags;
cvar_t *skill;
cvar_t *timelimit;
cvar_t *maxclients;
cvar_t *maxentities;
cvar_t *g_select_empty;
cvar_t *g_idle_time;
cvar_t *g_vote_mask;
cvar_t *g_vote_time;
cvar_t *g_vote_threshold;
cvar_t *g_vote_limit;
cvar_t *g_vote_flags;
cvar_t *g_intermission_time;
cvar_t *g_admin_password;
cvar_t *g_round_limit;
cvar_t *g_team_balance;
cvar_t *g_maps_random;
cvar_t *g_maps_file;
cvar_t *g_defaults_file;
cvar_t *g_item_ban;
cvar_t *g_bugs;
cvar_t *g_teleporter_nofreeze;
cvar_t *g_spawn_mode;
cvar_t *g_team_chat;
cvar_t *g_timeout_time;
cvar_t *g_mute_chat;
cvar_t *g_protection_time;
cvar_t *g_log_stats;
cvar_t *g_skins_file;
cvar_t *g_health_start;
cvar_t *g_armor_start;
cvar_t *dedicated;

#if USE_SQLITE
cvar_t *g_sql_database;
cvar_t *g_sql_async;
#endif

cvar_t *sv_maxvelocity;
cvar_t *sv_gravity;

cvar_t *sv_rollspeed;
cvar_t *sv_rollangle;
cvar_t *gun_x;
cvar_t *gun_y;
cvar_t *gun_z;

cvar_t *run_pitch;
cvar_t *run_roll;
cvar_t *bob_up;
cvar_t *bob_pitch;
cvar_t *bob_roll;

cvar_t *sv_cheats;
cvar_t *sv_hostname;

cvar_t *flood_msgs;
cvar_t *flood_persecond;
cvar_t *flood_waitdelay;

cvar_t *flood_waves;
cvar_t *flood_perwave;
cvar_t *flood_wavedelay;

cvar_t *flood_infos;
cvar_t *flood_perinfo;
cvar_t *flood_infodelay;

cvar_t *g_frag_drop;
cvar_t *g_round_end_time;
cvar_t *g_round_countdown;
cvar_t *g_team_count;
cvar_t *g_teamspec_name;
cvar_t *g_team1_name;
cvar_t *g_team1_skin;
cvar_t *g_team2_name;
cvar_t *g_team2_skin;
cvar_t *g_team3_name;
cvar_t *g_team3_skin;
cvar_t *g_team4_name;
cvar_t *g_team4_skin;
cvar_t *g_team5_name;
cvar_t *g_team5_skin;
cvar_t *g_default_arena;
cvar_t *g_weapon_flags;
cvar_t *g_damage_flags;
cvar_t *g_drop_allowed;
cvar_t *g_skin_lock;
cvar_t *g_ammo_slugs;
cvar_t *g_ammo_rockets;
cvar_t *g_ammo_cells;
cvar_t *g_ammo_grenades;
cvar_t *g_ammo_bullets;
cvar_t *g_ammo_shells;
cvar_t *g_timein_time;
cvar_t *g_screenshot;
cvar_t *g_demo;
cvar_t *g_team_reset;
cvar_t *g_all_chat;
cvar_t *g_round_timelimit;
cvar_t *g_fast_weapon_change;
cvar_t *g_debug_clocks;

LIST_DECL(g_map_list);
LIST_DECL(g_map_queue);

void ClientThink(edict_t *ent, usercmd_t *cmd);
qboolean ClientConnect(edict_t *ent, char *userinfo);
void ClientUserinfoChanged(edict_t *ent, char *userinfo);
void ClientDisconnect(edict_t *ent);
void ClientBegin(edict_t *ent);
void ClientCommand(edict_t *ent);

//===================================================================

/*
 =================
 ClientEndServerFrames

 Return number of connected cliets
 =================
*/
static void ClientEndServerFrames(void)
{
    int i;
    gclient_t *c;

    if (level.intermission_framenum) {
        // if the end of unit layout is displayed, don't give
        // the player any normal movement attributes
        for (i = 0, c = game.clients; i < game.maxclients; i++, c++) {
            if (!c->pers.team) {
                continue;
            }
            IntermissionEndServerFrame(c->edict);
        }
        return;
    }

    // calc the player views now that all pushing
    // and damage has been added
    for (i = 0, c = game.clients; i < game.maxclients; i++, c++) {
        if (!c->pers.team) {
            continue;
        }

        if (!c->chase_target) {
            ClientEndServerFrame(c->edict);
        }

        // if the scoreboard is up, update it
        if (c->layout == LAYOUT_SCORES && !(level.framenum % (3 * HZ))) {
            G_ArenaScoreboardMessage(c->edict, false);
        }

        PMenu_Update(c->edict);
    }

    // update chase cam after all stats and positions are calculated
    for (i = 0, c = game.clients; i < game.maxclients; i++, c++) {
        if (!IS_SPECTATOR(c->edict)) {
            continue;
        }
        if (c->chase_target) {
            ChaseEndServerFrame(c->edict);
        }
    }
}

static int ScoreCmp(const void *p1, const void *p2)
{
    score_t *a = (score_t *) p1;
    score_t *b = (score_t *) p2;

    if (a->score > b->score) {
        return -1;
    }

    if (a->score < b->score) {
        return 1;
    }

    if (a->time > b->time) {
        return -1;
    }

    if (a->time < b->time) {
        return 1;
    }

    return 0;
}

static void G_SaveScores(void)
{
    char    path[MAX_OSPATH];
    score_t *s;
    FILE    *fp;
    int     i;
    size_t  len;

    if (!game.dir[0]) {
        return;
    }

    len = Q_concat(path, sizeof(path), game.dir, "/highscores", NULL);
    if (len >= sizeof(path)) {
        return;
    }
    os_mkdir(path);

    len = Q_concat(path, sizeof(path), game.dir, "/highscores/", level.mapname,
            ".txt", NULL);
    if (len >= sizeof(path)) {
        return;
    }

    fp = fopen(path, "w");
    if (!fp) {
        return;
    }

    for (i = 0; i < level.numscores; i++) {
        s = &level.scores[i];
        fprintf(fp, "\"%s\" %d %lu\n", s->name, s->score,
                (unsigned long) s->time);
    }

    fclose(fp);
}

static void G_RegisterScore(void)
{
    gclient_t *ranks[MAX_CLIENTS];
    gclient_t *c;
    score_t   *s;
    int       total;
    int       sec,
              score;

    total = G_CalcRanks(ranks);
    if (!total) {
        return;
    }

    // grab our champion
    c = ranks[0];

    // calculate FPH
    sec = (level.framenum - c->resp.enter_framenum) / HZ;
    if (!sec) {
        sec = 1;
    }
    score = c->resp.score * 3600 / sec;

    if (score < 1) {
        return; // do not record bogus results
    }

    if (level.numscores < MAX_SCORES) {
        s = &level.scores[level.numscores++];
    } else {
        s = &level.scores[level.numscores - 1];
        if (score < s->score) {
            return; // result not impressive enough
        }
    }

    strcpy(s->name, c->pers.netname);
    s->score = score;
    time(&s->time);

    level.record = s->time;

    qsort(level.scores, level.numscores, sizeof(score_t), ScoreCmp);

    gi.dprintf("Added highscore entry for %s with %d FPH\n", c->pers.netname,
            score);

    G_SaveScores();
}

void G_LoadScores(void)
{
    char       path[MAX_OSPATH];
    char       buffer[MAX_STRING_CHARS];
    char       *token;
    const char *data;
    score_t    *s;
    FILE       *fp;
    size_t     len;

    if (!game.dir[0]) {
        return;
    }

    len = Q_concat(path, sizeof(path), game.dir, "/highscores/", level.mapname,
            ".txt", NULL);
    if (len >= sizeof(path)) {
        return;
    }

    fp = fopen(path, "r");
    if (!fp) {
        return;
    }

    do {
        data = fgets(buffer, sizeof(buffer), fp);
        if (!data) {
            break;
        }

        if (data[0] == '#' || data[0] == '/') {
            continue;
        }

        token = COM_Parse(&data);
        if (!*token) {
            continue;
        }

        s = &level.scores[level.numscores++];
        Q_strlcpy(s->name, token, sizeof(s->name));

        token = COM_Parse(&data);
        s->score = strtoul(token, NULL, 10);

        token = COM_Parse(&data);
        s->time = strtoul(token, NULL, 10);
    } while (level.numscores < MAX_SCORES);

    fclose(fp);

    qsort(level.scores, level.numscores, sizeof(score_t), ScoreCmp);

    gi.dprintf("Loaded %d scores from '%s'\n", level.numscores, path);
}

map_entry_t *G_FindMap(const char *name)
{
    map_entry_t *map;

    LIST_FOR_EACH(map_entry_t, map, &g_map_list, list)
    {
        if (!Q_stricmp(map->name, name)) {
            return map;
        }
    }

    return NULL;
}

static int G_RebuildMapQueue(void)
{
    map_entry_t **pool,
                *map;
    int         i,
                count;

    List_Init(&g_map_queue);

    count = List_Count(&g_map_list);
    if (!count) {
        return 0;
    }

    pool = G_Malloc(sizeof(map_entry_t *) * count);

    // build the queue from available map list
    count = 0;
    LIST_FOR_EACH(map_entry_t, map, &g_map_list, list)
    {
        if (map->flags & MAP_NOAUTO) {
            continue;
        }

        if (map->flags & MAP_EXCLUSIVE) {
            if (count && (pool[count - 1]->flags & MAP_WEIGHTED)) {
                continue;
            }
        } else if (map->flags & MAP_WEIGHTED) {
            if (random() > map->weight) {
                continue;
            }
        }
        pool[count++] = map;
    }

    if (!count) {
        goto done;
    }

    gi.dprintf("Map queue: %d entries\n", count);

    // randomize it

    if ((int) g_maps_random->value > 0) {
        G_ShuffleArray(pool, count);
    }

    for (i = 0; i < count; i++) {
        List_Append(&g_map_queue, &pool[i]->queue);
    }

    done: gi.TagFree(pool);
    return count;
}

static map_entry_t *G_FindSuitableMap(void)
{
    int total = G_CalcRanks(NULL);
    map_entry_t *map;

    LIST_FOR_EACH(map_entry_t, map, &g_map_queue, queue)
    {
        if (total >= map->min_players && total <= map->max_players) {
            if ((int) g_maps_random->value < 2
                    || strcmp(map->name, level.mapname)) {
                return map;
            }
        }
    }

    return NULL;
}

static void G_PickNextMap(void)
{
    map_entry_t *map;

    // if map list is empty, stay on the same level
    if (LIST_EMPTY(&g_map_list)) {
        return;
    }

    // pick the suitable map
    map = G_FindSuitableMap();
    if (!map) {
        // if map queue is empty, rebuild it
        if (!G_RebuildMapQueue()) {
            return;
        }

        map = G_FindSuitableMap();
        if (!map) {
            gi.dprintf("Couldn't find next map!\n");
            return;
        }
    }

    List_Delete(&map->queue);
    map->num_hits++;

    gi.dprintf("Next map is %s.\n", map->name);
    strcpy(level.nextmap, map->name);
}

/**
 * Read the map list file. These maps will be voteable in-game.
 */
static void G_LoadMapList(void)
{
    char        path[MAX_OSPATH];
    char        buffer[MAX_STRING_CHARS];
    char        *token;
    const char  *data;
    map_entry_t *map;
    FILE        *fp;
    size_t      len;
    int         linenum,
                nummaps;

    if (!game.dir[0]) {
        return;
    }

    len = Q_concat(path, sizeof(path), game.dir, "/mapcfg/", g_maps_file->string, NULL);
    if (len >= sizeof(path)) {
        return;
    }

    fp = fopen(path, "r");
    if (!fp) {
        gi.dprintf("Couldn't load '%s'...\n", path);
        return;
    }

    linenum = nummaps = 0;

    while (qtrue) {
        data = fgets(buffer, sizeof(buffer), fp);
        if (!data) {
            break;
        }

        linenum++;

        if (data[0] == '#' || data[0] == '/') {
            continue;
        }

        token = COM_Parse(&data);
        if (!*token) {
            continue;
        }

        len = strlen(token);
        if (len >= MAX_QPATH) {
            gi.dprintf("%s: oversize mapname at line %d\n", __func__, linenum);
            continue;
        }

        map = G_Malloc(sizeof(*map) + len);
        memcpy(map->name, token, len + 1);

        List_Append(&g_map_list, &map->list);
        nummaps++;
    }

    fclose(fp);
}

static void G_LoadSkinList(void)
{
    char         path[MAX_OSPATH];
    char         buffer[MAX_STRING_CHARS];
    char         *token;
    const char   *data;
    skin_entry_t *skin;
    FILE         *fp;
    size_t       len;
    int          linenum,
                 numskins,
                 numdirs;

    if (!game.dir[0]) {
        return;
    }

    len = Q_concat(path, sizeof(path), game.dir, "/", g_skins_file->string,
            ".txt", NULL);
    if (len >= sizeof(path)) {
        return;
    }

    fp = fopen(path, "r");
    if (!fp) {
        gi.dprintf("Couldn't load '%s'\n", path);
        return;
    }

    linenum = numskins = numdirs = 0;
    while (1) {
        data = fgets(buffer, sizeof(buffer), fp);
        if (!data) {
            break;
        }

        linenum++;

        if (data[0] == '#' || data[0] == '/') {
            continue;
        }

        token = COM_Parse(&data);
        if (!*token) {
            continue;
        }

        len = strlen(token);
        if (len >= MAX_SKINNAME) {
            gi.dprintf("%s: oversize skinname at line %d\n", __func__, linenum);
            continue;
        }

        skin = G_Malloc(sizeof(*skin) + len);
        memcpy(skin->name, token, len + 1);

        if (token[len - 1] == '/') {
            skin->name[len - 1] = 0;
            skin->next = game.skins;
            game.skins = skin;
            numdirs++;
        } else if (game.skins) {
            skin->next = game.skins->down;
            game.skins->down = skin;
            numskins++;
        } else {
            gi.dprintf("%s: skinname before directory at line %d\n", __func__,
                    linenum);
        }
    }

    fclose(fp);

    gi.dprintf("Loaded %d skins in %d dirs from '%s'\n", numskins, numdirs,
            path);
}

/*
 =================
 EndDMLevel

 The timelimit or fraglimit has been exceeded
 =================
 */
void G_EndLevel(void)
{
    uint8_t i;

    G_RegisterScore();

    for (i = 1; i <= level.arena_count; i++) {
        BeginIntermission(&level.arenas[i]);
    }

    if (level.nextmap[0]) {
        return; // already set by operator or vote
    }

    strcpy(level.nextmap, level.mapname);

    // stay on same level flag
    if (DF(SAME_LEVEL)) {
        return;
    }

    G_PickNextMap();
}

void G_StartSound(int index)
{
    //gi.sound(world, CHAN_RELIABLE, index, 1, ATTN_NONE, 0);
    //gi.sound(world, CHAN_AUTO, index, 1, ATTN_NORM, 0);    // doesn't work
}

void G_StuffText(edict_t *ent, const char *text)
{
    gi.WriteByte(SVC_STUFFTEXT);
    gi.WriteString(text);
    gi.unicast(ent, qtrue);
}

/**
 * Check gameplay rules
 */
static void G_CheckRules(void)
{
    if (g_vote_threshold->modified) {
        G_CheckVote();
        g_vote_threshold->modified = qfalse;
    }

    if (g_vote_flags->modified) {
        G_CheckVote();
        g_vote_flags->modified = qfalse;
    }
}

static void G_ResetSettings(void)
{
    char command[256];

    gi.bprintf(PRINT_HIGH,
            "No active players, restoring default game settings\n");

    if (g_defaults_file->string[0]) {
        Q_snprintf(command, sizeof(command), "exec \"%s\"\n",
                g_defaults_file->string);
        gi.AddCommandString(command);
    }

    game.settings_modified = 0;
}

/*
 =============
 ExitLevel
 =============
 */
void G_ExitLevel(void)
{
    char        command[256];
    map_entry_t *map;

    if (level.intermission_exit) {
        return; // already exited
    }

    map = G_FindMap(level.mapname);
    if (map) {
        map->num_in += level.players_in;
        map->num_out += level.players_out;
    }

    if (!level.nextmap || !strcmp(level.nextmap, level.mapname)) {
        G_ResetLevel();
        return;
    }

    Q_snprintf(command, sizeof(command), "gamemap \"%s\"\n", level.nextmap);
    gi.AddCommandString(command);

    level.intermission_exit = level.framenum;
}

/*
 ================
 G_RunFrame

 Advances the world by 0.1 seconds
 ================
 */
void G_RunFrame(void)
{
    int     i;
    edict_t *ent;
    arena_t *a;

    //
    // treat each object in turn
    // even the world gets a chance to think
    //
    for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++) {
        if (!ent->inuse)
            continue;

        level.current_entity = ent;

        VectorCopy(ent->old_origin, ent->s.old_origin);

        // if the ground entity moved, make sure we are still on it
        if ((ent->groundentity)
                && (ent->groundentity->linkcount != ent->groundentity_linkcount)) {
            ent->groundentity = NULL;
        }

        if (i > 0 && i <= game.maxclients) {
            ClientBeginServerFrame(ent);
            continue;
        }

        G_RunEntity(ent);
    }

    // see if it is time to end a deathmatch
    G_CheckRules();

    // let each arena think in turn
    FOR_EACH_ARENA(a) {
        G_ArenaThink(a);
    }

    // check vote timeout
    if (level.vote.proposal) {
        G_UpdateVote();
    }
    //}

    // build the playerstate_t structures for all players
    ClientEndServerFrames();

    // reset settings if no one was active for the last 5 minutes
    if (game.settings_modified
            && level.framenum - level.activity_framenum > 5 * 60 * HZ) {
        G_ResetSettings();
    }

    // save old_origins for next frame
    for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++) {
        if (ent->inuse) {
            VectorCopy(ent->s.origin, ent->old_origin);
        }
    }

    // advance for next frame
    level.framenum++;
    level.time = level.framenum * FRAMETIME;
    //ClockThink(&level.clock);
    if (level.clock.tick) {
        level.clock.tick(&level.clock);
    }
}

static void G_Shutdown(void)
{
    gi.dprintf("==== ShutdownGame ====\n");

#if USE_SQLITE
    if (game.clients) {
        G_LogClients();
    }
    G_CloseDatabase();
#endif

    gi.FreeTags(TAG_LEVEL);
    gi.FreeTags(TAG_GAME);

    memset(&game, 0, sizeof(game));

    // reset our features
    gi.cvar_forceset("g_features", "0");

    List_Init(&g_map_list);
    List_Init(&g_map_queue);
}

static void check_cvar(cvar_t *cv)
{
    if (strchr(cv->string, '/') || strstr(cv->string, "..")) {
        gi.dprintf("'%s' should be a single filename, not a path.\n", cv->name);
        gi.cvar_forceset(cv->name, "");
    }
}

/*
 ============
 InitGame

 This will be called when the dll is first loaded, which
 only happens when a new game is started or a save game
 is loaded.
 ============
 */
static void G_Init(void) {
    cvar_t *cv;
    size_t len;

    gi.dprintf("\n==== Game Init - %s %s ====\n", GAMEVERSION, OPENRA2_VERSION);

    gun_x = gi.cvar("gun_x", "0", 0);
    gun_y = gi.cvar("gun_y", "0", 0);
    gun_z = gi.cvar("gun_z", "0", 0);

    //FIXME: sv_ prefix is wrong for these
    sv_rollspeed = gi.cvar("sv_rollspeed", "200", 0);
    sv_rollangle = gi.cvar("sv_rollangle", "2", 0);
    sv_maxvelocity = gi.cvar("sv_maxvelocity", "2000", 0);
    sv_gravity = gi.cvar("sv_gravity", "800", 0);

    // noset vars
    dedicated = gi.cvar("dedicated", "0", CVAR_NOSET);

    // latched vars
    sv_cheats = gi.cvar("cheats", "0", CVAR_SERVERINFO | CVAR_LATCH);
    sv_hostname = gi.cvar("hostname", NULL, 0);
    gi.cvar("gamename", GAMEVERSION, CVAR_SERVERINFO);
    gi.cvar_set("gamename", GAMEVERSION);
    gi.cvar("gamedate", __DATE__, CVAR_SERVERINFO);
    gi.cvar_set("gamedate", __DATE__);

    maxclients = gi.cvar("maxclients", "4", CVAR_SERVERINFO | CVAR_LATCH);
    maxentities = gi.cvar("maxentities", "1024", CVAR_LATCH);

    // change anytime vars
    int df = DF_NO_HEALTH | DF_NO_ITEMS | DF_SKINTEAMS | DF_NO_ARMOR | DF_MODELTEAMS;
    dmflags = gi.cvar("dmflags", va("%d", df), CVAR_SERVERINFO);

    timelimit = gi.cvar("timelimit", "0", CVAR_SERVERINFO);

    gi.cvar("time_remaining", "", CVAR_SERVERINFO);
    gi.cvar_set("time_remaining", "");

    gi.cvar("revision", va("%d", OPENRA2_REVISION), CVAR_SERVERINFO);
    gi.cvar_set("revision", va("%d", OPENRA2_REVISION));

    g_select_empty = gi.cvar("g_select_empty", "0", CVAR_ARCHIVE);
    g_idle_time = gi.cvar("g_idle_time", "0", CVAR_GENERAL);
    g_vote_mask = gi.cvar("g_vote_mask", va("%d", VOTE_ALL), CVAR_GENERAL);
    g_vote_time = gi.cvar("g_vote_time", "30", CVAR_GENERAL);
    g_vote_threshold = gi.cvar("g_vote_threshold", "51", CVAR_GENERAL);
    g_vote_limit = gi.cvar("g_vote_limit", "100", CVAR_GENERAL);
    g_vote_flags = gi.cvar("g_vote_flags", va("%d", VF_DEFAULT), CVAR_GENERAL);
    g_intermission_time = gi.cvar("g_intermission_time", "10", CVAR_GENERAL);
    g_admin_password = gi.cvar("g_admin_password", "", CVAR_GENERAL);
    g_maps_random = gi.cvar("g_maps_random", "2", CVAR_GENERAL);
    g_maps_file = gi.cvar("g_maps_file", "maps.txt", CVAR_LATCH);
    g_defaults_file = gi.cvar("g_defaults_file", "", CVAR_LATCH);
    g_item_ban = gi.cvar("g_item_ban", "0", CVAR_GENERAL);
    g_bugs = gi.cvar("g_bugs", "0", CVAR_GENERAL);
    g_teleporter_nofreeze = gi.cvar("g_teleporter_nofreeze", "0", CVAR_GENERAL);
    g_spawn_mode = gi.cvar("g_spawn_mode", "1", CVAR_GENERAL);
    g_mute_chat = gi.cvar("g_mute_chat", "0", CVAR_GENERAL);
    g_protection_time = gi.cvar("g_protection_time", "0", CVAR_GENERAL);
#if USE_SQLITE
    g_sql_database = gi.cvar("g_sql_database", "", 0);
    g_sql_async = gi.cvar("g_sql_async", "0", 0);
#endif
    g_skins_file = gi.cvar("g_skins_file", "", CVAR_LATCH);

    run_pitch = gi.cvar("run_pitch", "0.002", CVAR_GENERAL);
    run_roll = gi.cvar("run_roll", "0.005", CVAR_GENERAL);
    bob_up = gi.cvar("bob_up", "0.005", CVAR_GENERAL);
    bob_pitch = gi.cvar("bob_pitch", "0.002", CVAR_GENERAL);
    bob_roll = gi.cvar("bob_roll", "0.002", CVAR_GENERAL);

    // chat flood control
    flood_msgs = gi.cvar("flood_msgs", "4", CVAR_GENERAL);
    flood_persecond = gi.cvar("flood_persecond", "4", CVAR_GENERAL);
    flood_waitdelay = gi.cvar("flood_waitdelay", "10", CVAR_GENERAL);

    // wave flood control
    flood_waves = gi.cvar("flood_waves", "4", CVAR_GENERAL);
    flood_perwave = gi.cvar("flood_perwave", "30", CVAR_GENERAL);
    flood_wavedelay = gi.cvar("flood_wavedelay", "60", CVAR_GENERAL);

    // userinfo flood control
    flood_infos = gi.cvar("flood_infos", "4", CVAR_GENERAL);
    flood_perinfo = gi.cvar("flood_perinfo", "30", CVAR_GENERAL);
    flood_infodelay = gi.cvar("flood_infodelay", "60", CVAR_GENERAL);

    // arena defaults
    g_round_end_time = gi.cvar("g_round_end_time", "5", CVAR_GENERAL);
    g_round_countdown = gi.cvar("g_round_countdown", "12", CVAR_GENERAL);

    // team stuff
    g_team_count = gi.cvar("g_team_count", "2", CVAR_LATCH);
    g_teamspec_name = gi.cvar("g_teamspec_name", "Spectators", CVAR_LATCH);
    g_team1_name = gi.cvar("g_team1_name", "Red", CVAR_LATCH);
    g_team1_skin = gi.cvar("g_team1_skin", "female/r2red", CVAR_LATCH);
    g_team2_name = gi.cvar("g_team2_name", "Blue", CVAR_LATCH);
    g_team2_skin = gi.cvar("g_team2_skin", "male/r2blue", CVAR_LATCH);
    g_team3_name = gi.cvar("g_team3_name", "Green", CVAR_LATCH);
    g_team3_skin = gi.cvar("g_team3_skin", "female/r2dgre", CVAR_LATCH);
    g_team4_name = gi.cvar("g_team4_name", "Yellow", CVAR_LATCH);
    g_team4_skin = gi.cvar("g_team4_skin", "male/r2yell", CVAR_LATCH);
    g_team5_name = gi.cvar("g_team5_name", "Aqua", CVAR_LATCH);
    g_team5_skin = gi.cvar("g_team5_skin", "female/r2aqua", CVAR_LATCH);

    g_default_arena = gi.cvar("g_default_arena", "1", CVAR_LATCH);
    g_weapon_flags = gi.cvar("g_weapon_flags", "1023", CVAR_LATCH);    // default: all except bfg
    g_damage_flags = gi.cvar("g_damage_flags",
            va("%d", ARENADAMAGE_SELF_ARMOR |ARENADAMAGE_FALL), CVAR_LATCH);
    g_drop_allowed = gi.cvar("g_drop_allowed", "1", CVAR_LATCH);
    g_skin_lock = gi.cvar("g_skin_lock", "0", CVAR_GENERAL);
    g_ammo_slugs = gi.cvar("g_ammo_slugs", "50", CVAR_LATCH);
    g_ammo_rockets = gi.cvar("g_ammo_rockets", "50", CVAR_LATCH);
    g_ammo_cells = gi.cvar("g_ammo_cells", "200", CVAR_LATCH);
    g_ammo_grenades = gi.cvar("g_ammo_grenades", "50", CVAR_LATCH);
    g_ammo_bullets = gi.cvar("g_ammo_bullets", "200", CVAR_LATCH);
    g_ammo_shells = gi.cvar("g_ammo_shells", "50", CVAR_LATCH);
    g_timein_time = gi.cvar("g_timein_time", "11", CVAR_GENERAL);
    g_timeout_time = gi.cvar("g_timeout_time", "180", CVAR_LATCH);
    g_round_limit = gi.cvar("g_round_limit", "7", CVAR_LATCH);
    g_team_balance = gi.cvar("g_team_balance", "0", CVAR_GENERAL);
    g_health_start = gi.cvar("g_health_start", "100", CVAR_LATCH);
    g_armor_start = gi.cvar("g_armor_start", "100", CVAR_LATCH);
    g_screenshot = gi.cvar("g_screenshot", "0", CVAR_GENERAL);
    g_demo = gi.cvar("g_demo", "0", CVAR_GENERAL);
    g_team_reset = gi.cvar("g_team_reset", "0", CVAR_GENERAL);
    g_team_chat = gi.cvar("g_team_chat", "0", CVAR_GENERAL);
    g_all_chat = gi.cvar("g_all_chat", "1", CVAR_GENERAL);
    g_frag_drop = gi.cvar("g_frag_drop", "0", CVAR_GENERAL);
    g_round_timelimit = gi.cvar("g_round_timelimit", "0", CVAR_GENERAL);
    g_fast_weapon_change = gi.cvar("g_fast_weapon_change", "1", CVAR_GENERAL);
    g_debug_clocks = gi.cvar("g_debug_clocks", "0", CVAR_GENERAL);

    // Sane limits
    clamp(g_round_countdown->value, 3, 30);
    clamp(g_round_end_time->value, 1, 15);
    clamp(g_weapon_flags->value, 0, 2046);
    clamp(g_damage_flags->value, 0, 31);
    clamp(g_timein_time->value, 3, 30);
    clamp(g_timeout_time->value, 30, 300);
    clamp(g_round_limit->value, 1, MAX_ROUNDS);
    clamp(g_health_start->value, 1, 999);
    clamp(g_armor_start->value, 0, 999);
    clamp(g_ammo_slugs->value, 0, 999);
    clamp(g_ammo_rockets->value, 0, 999);
    clamp(g_ammo_cells->value, 0, 999);
    clamp(g_ammo_grenades->value, 0, 999);
    clamp(g_ammo_bullets->value, 0, 999);
    clamp(g_ammo_shells->value, 0, 999);
    clamp(g_team_count->value, 2, MAX_TEAMS);

    // initialize all entities for this game
    game.maxentities = maxentities->value;
    clamp(game.maxentities, (int) maxclients->value + 1, MAX_EDICTS);
    g_edicts = G_Malloc(game.maxentities * sizeof(g_edicts[0]));
    globals.edicts = g_edicts;
    globals.max_edicts = game.maxentities;

    // initialize all clients for this game
    game.maxclients = maxclients->value;
    game.clients = G_Malloc(game.maxclients * sizeof(game.clients[0]));
    globals.num_edicts = game.maxclients + 1;

    // obtain game path
    cv = gi.cvar("fs_gamedir", NULL, 0);
    if (cv && cv->string[0]) {
        len = Q_strlcpy(game.dir, cv->string, sizeof(game.dir));
    } else {
        cvar_t *basedir = gi.cvar("basedir", NULL, 0);
        cvar_t *gamedir = gi.cvar("game", NULL, 0);
        if (basedir && gamedir) {
            len = Q_concat(game.dir, sizeof(game.dir), basedir->string, "/",
                    gamedir->string, NULL);
        } else {
            len = 0;
        }
    }

    if (!len) {
        gi.dprintf("Failed to determine game directory.\n");
    } else if (len >= sizeof(game.dir)) {
        gi.dprintf("Oversize game directory.\n");
        game.dir[0] = 0;
    }

    check_cvar(g_maps_file);
    if (g_maps_file->string[0]) {
        G_LoadMapList();
    }

    check_cvar(g_defaults_file);

#if USE_SQLITE
    check_cvar(g_sql_database);
#endif

    check_cvar(g_skins_file);
    if (g_skins_file->string[0]) {
        G_LoadSkinList();
    }

    // obtain server features
    cv = gi.cvar("sv_features", NULL, 0);
    if (cv) {
        game.serverFeatures = (int) cv->value;
    }

#if USE_FPS
    // setup framerate parameters
    if (game.serverFeatures & GMF_VARIABLE_FPS) {
        int framediv;

        cv = gi.cvar("sv_fps", NULL, 0);
        if (!cv)
        gi.error("GMF_VARIABLE_FPS exported but no 'sv_fps' cvar");

        framediv = (int)cv->value / BASE_FRAMERATE;

        clamp(framediv, 1, MAX_FRAMEDIV);

        game.framerate = framediv * BASE_FRAMERATE;
        game.frametime = BASE_FRAMETIME_1000 / framediv;
        game.framediv = framediv;
    } else {
        game.framerate = BASE_FRAMERATE;
        game.frametime = BASE_FRAMETIME_1000;
        game.framediv = 1;
    }
#endif

    // export our own features
    gi.cvar_forceset("g_features", va("%d", G_FEATURES));

    gi.dprintf("==== Game Initialized ====\n\n");
}

static void G_WriteGame(const char *filename, qboolean autosave) {}
static void G_ReadGame(const char *filename) {}
static void G_WriteLevel(const char *filename) {}
static void G_ReadLevel(const char *filename) {}

//======================================================================

#ifndef GAME_HARD_LINKED

// this is only here so the functions in q_shared.c can link
void Com_LPrintf(print_type_t type, const char *fmt, ...)
{
    va_list argptr;
    char text[MAX_STRING_CHARS];

    va_start(argptr, fmt);
    Q_vsnprintf(text, sizeof(text), fmt, argptr);
    va_end(argptr);

    gi.dprintf("%s", text);
}

void Com_Error(error_type_t code, const char *error, ...)
{
    va_list argptr;
    char text[MAX_STRING_CHARS];

    va_start(argptr, error);
    Q_vsnprintf(text, sizeof(text), error, argptr);
    va_end(argptr);

    gi.error("%s", text);
}

#endif

/*
 =================
 GetGameAPI

 Returns a pointer to the structure with all entry points
 and global variables
 =================
 */
q_exported game_export_t *GetGameAPI(game_import_t *import)
{
    gi = *import;

    globals.apiversion = GAME_API_VERSION;
    globals.Init = G_Init;
    globals.Shutdown = G_Shutdown;
    globals.SpawnEntities = G_SpawnEntities;

    globals.WriteGame = G_WriteGame;
    globals.ReadGame = G_ReadGame;
    globals.WriteLevel = G_WriteLevel;
    globals.ReadLevel = G_ReadLevel;

    globals.ClientThink = ClientThink;
    globals.ClientConnect = ClientConnect;
    globals.ClientUserinfoChanged = ClientUserinfoChanged;
    globals.ClientDisconnect = ClientDisconnect;
    globals.ClientBegin = ClientBegin;
    globals.ClientCommand = ClientCommand;

    globals.RunFrame = G_RunFrame;

    globals.ServerCommand = G_ServerCommand;

    globals.edict_size = sizeof(edict_t);

    return &globals;
}

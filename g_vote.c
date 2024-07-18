/*
Copyright (C) 2008-2009 Andrey Nazarov

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

//the ordering of weapons must match ITEM_ defines too!
const weaponinfo_t weaponvotes[WEAPON_MAX] =
{
    {{"shot", "sg"},    ARENAWEAPON_SHOTGUN, ITEM_SHOTGUN, ITEM_SHELLS},
    {{"sup", "ssg"},    ARENAWEAPON_SUPERSHOTGUN, ITEM_SUPERSHOTGUN, ITEM_SHELLS},
    {{"mac", "mg"},     ARENAWEAPON_MACHINEGUN, ITEM_MACHINEGUN, ITEM_BULLETS},
    {{"cha", "cg"},     ARENAWEAPON_CHAINGUN, ITEM_CHAINGUN, ITEM_BULLETS},
    {{"han", "hg"},     ARENAWEAPON_GRENADE, ITEM_GRENADES, ITEM_GRENADES},
    {{"gre", "gl"},     ARENAWEAPON_GRENADELAUNCHER, ITEM_GRENADELAUNCHER, ITEM_GRENADES},
    {{"roc", "rl"},     ARENAWEAPON_ROCKETLAUNCHER, ITEM_ROCKETLAUNCHER, ITEM_ROCKETS},
    {{"hyper", "hb"},   ARENAWEAPON_HYPERBLASTER, ITEM_HYPERBLASTER, ITEM_CELLS},
    {{"rail", "rg"},    ARENAWEAPON_RAILGUN, ITEM_RAILGUN, ITEM_SLUGS},
    {{"bfg", "10k"},    ARENAWEAPON_BFG, ITEM_BFG, ITEM_CELLS},
};

const damagevote_t damagevotes[DAMAGE_MAX] =
{
    {"self",    ARENADAMAGE_SELF},
    {"aself",   ARENADAMAGE_SELF_ARMOR},
    {"team",    ARENADAMAGE_TEAM},
    {"ateam",   ARENADAMAGE_TEAM_ARMOR},
    {"falling", ARENADAMAGE_FALL},
};


// voting enabled and on a team or an admin
#define MAY_VOTE(c) \
    (VF(ENABLED) && (c->pers.team || c->pers.admin))

void G_FinishArenaVote(arena_t *a) {
    if (VF(SHOW)) {
        G_ConfigString(a, CS_VOTE_PROPOSAL, "");
    }
    a->vote.proposal = 0;
    a->vote.framenum = level.framenum;
    a->vote.victim = NULL;
}

static int G_CalcVote(int *votes, arena_t *a)
{
    gclient_t *c;
    uint8_t i;
    int total = 0;

    votes[0] = votes[1] = 0;

    if (a && a->vote.proposal) {
        for (i=0; i<a->client_count; i++) {
            if (!a->clients[i])
                continue;

            if (!a->clients[i]->client)
                continue;

            c = a->clients[i]->client;

            if (c->pers.mvdspec) {
                continue;
            }

            if (!MAY_VOTE(c)) {
                continue; // don't count spectators
            }

            total++;
            if (c->level.vote.index != a->vote.index) {
                continue; // not voted yet
            }

            if (c->pers.admin) {
                // admin vote decides immediately
                votes[c->level.vote.accepted    ] = game.maxclients;
                votes[c->level.vote.accepted ^ 1] = 0;
                break;
            }

            // count normal vote
            votes[c->level.vote.accepted]++;
        }
    } else {
        for (c = game.clients; c < game.clients + game.maxclients; c++) {
            if (c->pers.mvdspec) {
                continue;
            }
            if (!MAY_VOTE(c)) {
                continue; // don't count spectators
            }

            total++;
            if (c->level.vote.index != level.vote.index) {
                continue; // not voted yet
            }

            if (c->pers.admin) {
                // admin vote decides immediately
                votes[c->level.vote.accepted    ] = game.maxclients;
                votes[c->level.vote.accepted ^ 1] = 0;
                break;
            }

            // count normal vote
            votes[c->level.vote.accepted]++;
        }
    }

    return total;
}

static int _G_CalcVote(int *votes, arena_t *a)
{
    int threshold = (int) g_vote_threshold->value;
    int total = G_CalcVote(votes, a);

    total = total * threshold / 100 + 1;

    return total;
}

void G_FinishVote(void)
{
    if (VF(SHOW)) {
        gi.configstring(CS_VOTE_PROPOSAL, "");
    }
    level.vote.proposal = 0;
    level.vote.framenum = level.framenum;
    level.vote.victim = NULL;
}

/**
 * Arena specific votes
 */
void G_CheckVoteStatus(arena_t *a) {
    char buffer[MAX_QPATH];
    uint8_t total;
    uint32_t remaining;
    int votes[2];

    if (!a) {
        return;
    }

    if (!a->vote.proposal) {
        return;
    }

    // check timeout
    if (level.framenum >= a->vote.framenum) {
        G_bprintf(a, PRINT_HIGH, "Local vote timed out.\n");
        G_FinishArenaVote(a);
        return;
    }

    if (!VF(SHOW)) {
        return;
    }

    remaining = a->vote.framenum - level.framenum;
    if (remaining % HZ) {
        return;
    }

    total = _G_CalcVote(votes, a);
    Q_snprintf(buffer, sizeof(buffer), "Yes: %d (%d) No: %d [%02d sec]",
               votes[1], total, votes[0], remaining / HZ);

    G_ConfigString(a, CS_VOTE_COUNT, buffer);
}

/**
 * Global votes
 */
void G_UpdateVote(void)
{
    char buffer[MAX_QPATH];
    int votes[2], total;
    int remaining;

    if (!level.vote.proposal) {
        return;
    }

    // check timeout
    if (level.framenum >= level.vote.framenum) {
        gi.bprintf(PRINT_HIGH, "Vote timed out.\n");
        G_FinishVote();
        return;
    }

    // update vote count every second
    if (!VF(SHOW)) {
        return;
    }

    remaining = level.vote.framenum - level.framenum;
    if (remaining % HZ) {
        return;
    }

    total = _G_CalcVote(votes, NULL);
    Q_snprintf(buffer, sizeof(buffer), "Yes: %d (%d) No: %d [%02d sec]",
               votes[1], total, votes[0], remaining / HZ);

    gi.WriteByte(SVC_CONFIGSTRING);
    gi.WriteShort(CS_VOTE_COUNT);
    gi.WriteString(buffer);
    gi.multicast(NULL, MULTICAST_ALL);
}

qboolean G_CheckVote(void)
{
    int threshold = (int) g_vote_threshold->value;
    int votes[2], total;
    int acc, rej;

    if (!level.vote.proposal) {
        return qfalse;
    }

    // is vote initiator gone?
    if (!level.vote.initiator->pers.connected) {
        gi.bprintf(PRINT_HIGH, "Vote aborted due to the initiator disconnect.\n");
        goto finish;
    }

    // is vote victim gone?
    if (level.vote.victim && !level.vote.victim->pers.connected) {
        gi.bprintf(PRINT_HIGH, "Vote aborted due to the victim disconnect.\n");
        goto finish;
    }

    // are there any players?
    total = G_CalcVote(votes, NULL);
    if (!total) {
        gi.bprintf(PRINT_HIGH, "Vote aborted due to the absence of players.\n");
        goto finish;
    }

    rej = votes[0] * 100 / total;
    acc = votes[1] * 100 / total;

    if (acc > threshold) {
        switch (level.vote.proposal) {

        case VOTE_KICK:
            gi.bprintf(PRINT_HIGH, "Vote passed.\n");
            gi.AddCommandString(va("kick %d\n", (int) (level.vote.victim - game.clients)));
            break;
        case VOTE_MUTE:
            gi.bprintf(PRINT_HIGH, "Vote passed: %s has been muted\n", level.vote.victim->pers.netname);
            level.vote.victim->pers.muted = qtrue;
            break;
        case VOTE_MAP:
            gi.bprintf(PRINT_HIGH, "Vote passed: next map is %s.\n", level.vote.map);
            gi.AddCommandString(va("gamemap %s", level.vote.map));
            break;

        default:
            break;
        }
        goto finish;
    }

    if (rej > threshold) {
        gi.bprintf(PRINT_HIGH, "Vote failed.\n");
        goto finish;
    }

    return qfalse;

finish:
    G_FinishVote();
    return qtrue;
}

qboolean G_CheckArenaVote(arena_t *a)
{
    int threshold = (int) g_vote_threshold->value;
    int votes[2], total;
    int acc, rej;

    if (!a) {
        return qfalse;
    }

    if (!a->vote.proposal) {
        return qfalse;
    }

    // is vote initiator gone?
    if (!a->vote.initiator->client->pers.connected) {
        G_bprintf(a, PRINT_HIGH, "Vote aborted due to the initiator disconnect.\n");
        goto finish;
    }

    // is vote victim gone?
    if (a->vote.victim && !a->vote.victim->client->pers.connected) {
        G_bprintf(a, PRINT_HIGH, "Vote aborted due to the victim disconnect.\n");
        goto finish;
    }

    // are there any players?
    total = G_CalcVote(votes, a);
    if (!total) {
        G_bprintf(a, PRINT_HIGH, "Vote aborted due to the absence of players.\n");
        goto finish;
    }

    rej = votes[0] * 100 / total;
    acc = votes[1] * 100 / total;

    uint32_t from;

    if (acc > threshold) {
        switch (a->vote.proposal) {

        case VOTE_TEAMS:
            a->modified = qtrue;
            from = a->team_count;
            a->team_count = a->vote.value;
            G_bprintf(a, PRINT_HIGH, "Local vote passed: team count changed from %d to %d\n", from, a->vote.value);
            G_RecreateArena(a);
            break;

        case VOTE_HEALTH:
            a->modified = qtrue;
            from = a->health;
            a->health = a->vote.value;
            G_RefillPlayers(a);
            G_bprintf(a, PRINT_HIGH, "Local vote passed: max health changed from %d to %d\n", from, a->vote.value);
            break;

        case VOTE_ARMOR:
            a->modified = qtrue;
            from = a->armor;
            a->armor = a->vote.value;
            G_RefillPlayers(a);
            G_bprintf(a, PRINT_HIGH, "Local vote passed: max armor changed from %d to %d\n", from, a->vote.value);
            break;

        case VOTE_ROUNDS:
            a->modified = qtrue;
            from = a->round_limit;
            a->round_limit = a->vote.value;
            G_bprintf(a, PRINT_HIGH, "Local vote passed: round count changed from %d to %d\n", from, a->vote.value);
            break;

        case VOTE_WEAPONS:
            a->modified = qtrue;
            a->weapon_flags = a->vote.value;

            memcpy(a->ammo, a->vote.items, sizeof(a->ammo));
            memcpy(a->infinite, a->vote.infinite, sizeof(a->infinite));

            G_RefillPlayers(a);
            G_UpdatePlayerStatusBars(a);
            G_bprintf(a, PRINT_HIGH, "Local vote passed: weapons changed to '%s'\n", G_WeaponFlagsToString(a));
            break;

        case VOTE_DAMAGE:
            a->modified = qtrue;
            a->damage_flags = a->vote.value;
            G_bprintf(a, PRINT_HIGH, "Local vote passed: damage protection changed to '%s'\n", G_DamageFlagsToString(a->damage_flags));
            break;

        case VOTE_RESET:
            G_ApplyDefaults(a);
            G_bprintf(a, PRINT_HIGH, "Local vote passed: all settings reset\n");
            break;

        case VOTE_SWITCH:
            a->modified = qtrue;
            a->fastswitch = a->vote.value;
            G_bprintf(a, PRINT_HIGH, "Local vote passed: fast weapon switching now %sabled\n", (a->vote.value) ? "en" : "dis");
            break;

        case VOTE_TIMELIMIT:
            a->modified = qtrue;
            a->timelimit = a->vote.value;
            G_bprintf(a, PRINT_HIGH, "Local vote passed: timelimit %d \n", a->vote.value);
            break;

        case VOTE_MODE:
            a->modified = qtrue;
            a->mode = a->vote.value;
            G_bprintf(a, PRINT_HIGH, "Local vote passed: gameplay mode changed to %s \n", G_ArenaModeString(a));
            break;

        case VOTE_CORPSE:
            a->modified = qtrue;
            a->corpseview = a->vote.value;
            G_bprintf(a, PRINT_HIGH, "Local vote passed: corpse view now %s\n", (a->corpseview) ? "ON" : "OFF");
            G_RecreateArena(a);
            break;

        case VOTE_CONFIG:
            a->modified = qtrue;
            gi.AddCommandString(va("exec config/%s.cfg\n", a->vote.map));
            G_bprintf(a, PRINT_HIGH, "Local vote passed: local config file applied\n");
            break;

        default:
            break;
        }
        goto finish;
    }

    if (rej > threshold) {
        G_bprintf(a, PRINT_HIGH, "Vote failed.\n");
        goto finish;
    }

    return qfalse;

finish:
    G_FinishArenaVote(a);
    return qtrue;
}

static void G_BuildProposal(char *buffer, arena_t *a)
{
    uint32_t proposal;
    proposal = (level.vote.proposal) ? level.vote.proposal : (a->vote.proposal) ? a->vote.proposal : 0;

    switch (proposal) {

    case VOTE_KICK:
        sprintf(buffer, "kick %s", level.vote.victim->pers.netname);
        break;
    case VOTE_MUTE:
        sprintf(buffer, "mute %s", level.vote.victim->pers.netname);
        break;
    case VOTE_MAP:
        sprintf(buffer, "map %s", level.vote.map);
        break;
    case VOTE_TEAMS:
        sprintf(buffer, "teams %d", a->vote.value);
        break;
    case VOTE_WEAPONS:
        sprintf(buffer, "%s", a->vote.original);
        break;
    case VOTE_DAMAGE:
        sprintf(buffer, "%s", a->vote.original);
        break;
    case VOTE_ROUNDS:
        sprintf(buffer, "rounds %d", a->vote.value);
        break;
    case VOTE_HEALTH:
        sprintf(buffer, "health %d", a->vote.value);
        break;
    case VOTE_ARMOR:
        sprintf(buffer, "armor %d", a->vote.value);
        break;
    case VOTE_RESET:
        sprintf(buffer, "reset all settings");
        break;
    case VOTE_SWITCH:
        sprintf(buffer, "%sable fast weapon switching", (a->vote.value) ? "En" : "Dis");
        break;
    case VOTE_TIMELIMIT:
        sprintf(buffer, "timelimit %d", a->vote.value);
        break;
    case VOTE_MODE:
        sprintf(buffer, "%s mode", (a->vote.value) ? "competition" : "normal");
        break;
    case VOTE_CORPSE:
        sprintf(buffer, "corpse-view %s", (a->vote.value) ? "on" : "off");
        break;
    case VOTE_CONFIG:
        snprintf(buffer, MAX_QPATH + 20, "apply local config: %s", a->vote.map);
        break;
    default:
        strcpy(buffer, "unknown");
        break;
    }
}

void Cmd_CastVote_f(edict_t *ent, qboolean accepted)
{
    if (!level.vote.proposal && !ARENA(ent)->vote.proposal) {
        gi.cprintf(ent, PRINT_HIGH, "No vote in progress.\n");
        return;
    }

    if (!MAY_VOTE(ent->client)) {
        gi.cprintf(ent, PRINT_HIGH, "Only team players can vote.\n");
        return;
    }

    if (ent->client->level.vote.index == level.vote.index && level.vote.proposal) {
        if (ent->client->level.vote.accepted == accepted) {
            gi.cprintf(ent, PRINT_HIGH, "You've already voted %s.\n", accepted ? "YES" : "NO");
            return;
        }

        gi.bprintf(PRINT_HIGH, "%s changed his vote to %s.\n", NAME(ent), accepted ? "YES" : "NO");

    } else if (ent->client->level.vote.index == ARENA(ent)->vote.index && ARENA(ent)->vote.proposal) {
        if (ent->client->level.vote.accepted == accepted) {
            gi.cprintf(ent, PRINT_HIGH, "You've already voted %s.\n", accepted ? "YES" : "NO");
            return;
        }

        gi.bprintf(PRINT_HIGH, "%s changed his vote to %s.\n", NAME(ent), accepted ? "YES" : "NO");
    } else {
        gi.bprintf(PRINT_HIGH, "%s voted %s.\n", NAME(ent), accepted ? "YES" : "NO");
    }

    ent->client->level.vote.index = (ARENA(ent)->vote.proposal) ? ARENA(ent)->vote.index : level.vote.index;
    ent->client->level.vote.accepted = accepted;

    G_CheckVote();
    G_CheckArenaVote(ARENA(ent));
}

uint8_t weapon_vote_index(const char *name) {
    uint8_t i;
    for (i=0; i<WEAPON_MAX; i++) {
        if (str_equal(name, weaponvotes[i].names[0]) || str_equal(name, weaponvotes[i].names[1])) {
            return i;
        }
    }

    return -1;
}

uint8_t damage_vote_index(const char *name)
{
    uint8_t i;
    for (i=0; i<DAMAGE_MAX; i++) {
        if (str_equal(name, damagevotes[i].name)) {
            return i;
        }
    }

    return -1;
}

static qboolean vote_victim(edict_t *ent)
{
    edict_t *other = G_SetVictim(ent, 1);
    if (!other) {
        return qfalse;
    }

    level.vote.victim = other->client;
    return qtrue;
}

static qboolean vote_map(edict_t *ent)
{
    char *name = gi.argv(2);
    map_entry_t *map;

    map = G_FindMap(name);
    if (!map) {
        gi.cprintf(ent, PRINT_HIGH, "Map '%s' is not available on this server.\n", name);
        return qfalse;
    }

    if (map->flags & MAP_NOVOTE) {
        gi.cprintf(ent, PRINT_HIGH, "Map '%s' is not available for voting.\n", map->name);
        return qfalse;
    }

    if (!strcmp(level.mapname, map->name)) {
        gi.cprintf(ent, PRINT_HIGH, "You are already playing '%s'.\n", map->name);
        return qfalse;
    }

    strcpy(level.vote.map, map->name);
    return qtrue;
}

static qboolean vote_teams(edict_t *ent)
{
    char *arg = gi.argv(2);
    unsigned count = strtoul(arg, NULL, 10);
    clamp(count, 2, MAX_TEAMS);
    ARENA(ent)->vote.value = count;
    return qtrue;
}

static qboolean vote_weapons(edict_t *ent)
{
    const char *input = gi.args();
    arena_t *arena = ARENA(ent);
    temp_weaponflags_t temp;

    strncpy(arena->vote.original, input, sizeof(arena->vote.original));

    COM_Parse(&input);  // remove "weapon" from command

    // start the vote with what we've already got
    arena->vote.value = arena->weapon_flags;
    memcpy(arena->vote.items, arena->ammo, sizeof(arena->vote.items));
    memcpy(arena->vote.infinite, arena->infinite, sizeof(arena->vote.infinite));

    if (G_ParseWeaponString(arena, ent, &input, &temp)) {
        arena->vote.value = temp.weaponflags;
        memcpy(arena->vote.items, temp.ammo, sizeof(arena->vote.items));
        memcpy(arena->vote.infinite, temp.infinite, sizeof(arena->vote.infinite));
        return qtrue;
    }

    return qfalse;
}

static qboolean vote_damage(edict_t *ent)
{
    const char *input = gi.args();
    arena_t *arena = ARENA(ent);
    uint32_t output = 0;

    arena->vote.value = arena->damage_flags;

    strncpy(arena->vote.original, input, sizeof(arena->vote.original));

    COM_Parse(&input);  // get rid of the word "weapon" from the head

    if (G_ParseDamageString(arena, ent, &input, &output)) {
        arena->vote.value = output;
        return qtrue;
    }

    return qfalse;
}

static qboolean vote_rounds(edict_t *ent)
{
    char *arg = gi.argv(2);
    unsigned count = strtoul(arg, NULL, 10);
    clamp(count, 1, MAX_ROUNDS);
    ARENA(ent)->vote.value = count;
    return qtrue;
}

static qboolean vote_health(edict_t *ent)
{
    char *arg = gi.argv(2);
    unsigned count = strtoul(arg, NULL, 10);
    clamp(count, 1, 999);
    ARENA(ent)->vote.value = count;
    return qtrue;
}

static qboolean vote_armor(edict_t *ent)
{
    char *arg = gi.argv(2);
    unsigned count = strtoul(arg, NULL, 10);
    clamp(count, 0, 999);
    ARENA(ent)->vote.value = count;
    return qtrue;
}

static qboolean vote_reset(edict_t *ent)
{
    return true;
}

static qboolean vote_fastswitch(edict_t *ent)
{
    char *arg = gi.argv(2);
    unsigned value = strtoul(arg, NULL, 10);
    clamp(value, 0, 1);
    ARENA(ent)->vote.value = value;
    return qtrue;
}

static qboolean vote_timelimit(edict_t *ent)
{
    char *arg = gi.argv(2);
    unsigned count = strtoul(arg, NULL, 10);
    clamp(count, 0, 20);
    ARENA(ent)->vote.value = count;
    return qtrue;
}

static qboolean vote_mode(edict_t *ent)
{
    char *arg = gi.argv(2);
    unsigned count = strtoul(arg, NULL, 10);
    clamp(count, 0, ARENA_MODE_MAX);
    ARENA(ent)->vote.value = count;
    return qtrue;
}

static qboolean vote_corpseview(edict_t *ent)
{
    char *arg = gi.argv(2);
    unsigned count = strtoul(arg, NULL, 10);
    clamp(count, 0, 1);
    ARENA(ent)->vote.value = count;
    return qtrue;
}

/**
 * Check if config voting command is valid or not.
 *
 * Doesn't check if requested config file exists in the filesystem, only if
 * it's been defined in the g_configlist cvar.
 */
static qboolean vote_config(edict_t *ent) {
    localconfig_t *c;
    qboolean found = qfalse;
    char *arg = gi.argv(2);

    if (LIST_EMPTY(&configlist)) {
        gi.cprintf(ent, PRINT_HIGH, "No server configs are available");
        return qfalse;
    }

    // Make sure config name requested is in the list
    FOR_EACH_CONFIG(c) {
        if (!Q_stricmp(c->config, arg)) {
            found = qtrue;
        }
    }
    if (!found) {
        gi.cprintf(ent, PRINT_HIGH, "config '%s' not allowed\n", arg);
        return qfalse;
    }

    // Just use the map field for the config file name
    strncpy(ARENA(ent)->vote.map, arg, MAX_QPATH);
    return qtrue;
}

typedef struct {
    const char  *name;
    int         bit;
    qboolean    (*func)(edict_t *);
} vote_proposal_t;

static const vote_proposal_t vote_proposals[] = {
    {"kick",        VOTE_KICK,      vote_victim},
    {"mute",        VOTE_MUTE,      vote_victim},
    {"map",         VOTE_MAP,       vote_map},
    {"teams",       VOTE_TEAMS,     vote_teams},
    {"weapons",     VOTE_WEAPONS,   vote_weapons},
    {"damage",      VOTE_DAMAGE,    vote_damage},
    {"rounds",      VOTE_ROUNDS,    vote_rounds},
    {"health",      VOTE_HEALTH,    vote_health},
    {"armor",       VOTE_ARMOR,     vote_armor},
    {"reset",       VOTE_RESET,     vote_reset},
    {"fastswitch",  VOTE_SWITCH,    vote_fastswitch},
    {"timelimit",   VOTE_TIMELIMIT, vote_timelimit},
    {"mode",        VOTE_MODE,      vote_mode},
    {"corpseview",  VOTE_CORPSE,    vote_corpseview},
    {"config",      VOTE_CONFIG,    vote_config},
    {NULL}
};

/**
 * A client used the "vote" command in console...
 */
void Cmd_Vote_f(edict_t *ent)
{
    char buffer[MAX_STRING_CHARS];
    const vote_proposal_t *v;
    int mask = g_vote_mask->value;
    int limit = g_vote_limit->value;
    int argc = gi.argc();
    int votes[2], total;
    char *s;
    arena_t *a;

    a = ARENA(ent);

    if (!VF(ENABLED)) {
        gi.cprintf(ent, PRINT_HIGH, "Voting is disabled on this server.\n");
        return;
    }

    if (argc < 2) {
        if (!level.vote.proposal && !ARENA(ent)->vote.proposal) {
            gi.cprintf(ent, PRINT_HIGH, "No vote in progress. Type '%s help' for usage.\n", gi.argv(0));
            return;
        }
        G_BuildProposal(buffer, a);
        total = _G_CalcVote(votes, a);
        if (a->vote.proposal) {
            gi.cprintf(ent, PRINT_HIGH,
                       "Proposal:   %s\n"
                       "Yes/No:     %d/%d [%d]\n"
                       "Timeout:    %d sec\n"
                       "Initiator:  %s\n",
                       buffer, votes[1], votes[0], total,
                       (a->vote.framenum - level.framenum) / HZ,
                       a->vote.initiator->client->pers.netname);
        } else {
            gi.cprintf(ent, PRINT_HIGH,
                       "Proposal:   %s\n"
                       "Yes/No:     %d/%d [%d]\n"
                       "Timeout:    %d sec\n"
                       "Initiator:  %s\n",
                       buffer, votes[1], votes[0], total,
                       (level.vote.framenum - level.framenum) / HZ,
                       level.vote.initiator->pers.netname);
        }
        return;
    }

    s = gi.argv(1);

//
// generic commands
//
    if (!strcmp(s, "help") || !strcmp(s, "h")) {
        gi.cprintf(ent, PRINT_HIGH,
                   "Usage: %s [yes/no/help/proposal] [arguments]\n"
                   "Available proposals:\n", gi.argv(0));

        if (mask & VOTE_KICK) {
            gi.cprintf(ent, PRINT_HIGH,
                       " kick <player_id>               Kick player from the server\n");
        }
        if (mask & VOTE_MUTE) {
            gi.cprintf(ent, PRINT_HIGH,
                       " mute <player_id>               Disallow player to talk\n");
        }
        if (mask & VOTE_MAP) {
            gi.cprintf(ent, PRINT_HIGH,
                       " map <name>                     Change current map\n");
        }
        if (mask & VOTE_TEAMS) {
            gi.cprintf(ent, PRINT_HIGH,
                       " teams <2-%d>                    Change the number of teams for this arena\n", MAX_TEAMS);
        }
        if (mask & VOTE_WEAPONS) {
            gi.cprintf(ent, PRINT_HIGH,
                       " weapons <weapflags>            Change available weapons for this arena\n");
        }
        if (mask & VOTE_DAMAGE) {
            gi.cprintf(ent, PRINT_HIGH,
                       " damage <dmgflags>              Change damage flags (falling dmg/friendly fire/armor)\n");
        }
        if (mask & VOTE_ROUNDS) {
            gi.cprintf(ent, PRINT_HIGH,
                       " rounds <count>                 Change the number of rounds per match for this arena\n");
        }
        if (mask & VOTE_HEALTH) {
            gi.cprintf(ent, PRINT_HIGH,
                       " health <count>                 Change the starting health for everyone in this arena\n");
        }
        if (mask & VOTE_ARMOR) {
            gi.cprintf(ent, PRINT_HIGH,
                       " armor <count>                  Change the starting armor for everyone in this arena\n");
        }
        if (mask & VOTE_RESET) {
            gi.cprintf(ent, PRINT_HIGH,
                       " reset                          Set all arena settings back to default\n");
        }
        if (mask & VOTE_SWITCH) {
            gi.cprintf(ent, PRINT_HIGH,
                       " fastswitch <0/1>               Disable or enable fast weapon switching\n");
        }
        if (mask & VOTE_TIMELIMIT) {
            gi.cprintf(ent, PRINT_HIGH,
                       " timelimit <minutes>            Limit rounds to this amount of time\n");
        }
        if (mask & VOTE_MODE) {
            gi.cprintf(ent, PRINT_HIGH,
                       " mode <0/1>                     Gameplay mode, 0=regular, 1=competition\n");
        }
        if (mask & VOTE_CONFIG) {
            gi.cprintf(ent, PRINT_HIGH,
                       " config <configname>            exec a local config\n");
        }

        // leave corpseview out of help

        gi.cprintf(ent, PRINT_HIGH,
                   "Available commands:\n"
                   " yes/no                         Accept/deny current vote\n"
                   " help                           Show this help\n");
        return;
    }

    if (!strcmp(s, "yes") || !strcmp(s, "y") || !strcmp(s, "1")) {
        Cmd_CastVote_f(ent, qtrue);
        return;
    }

    if (!strcmp(s, "no") || !strcmp(s, "n") || !strcmp(s, "0")) {
        Cmd_CastVote_f(ent, qfalse);
        return;
    }

//
// proposals
//
    if (level.vote.proposal) {
        gi.cprintf(ent, PRINT_HIGH, "Vote is already in progress.\n");
        return;
    }

    if (!MAY_VOTE(ent->client)) {
        gi.cprintf(ent, PRINT_HIGH, "Spectators can't vote on this server.\n");
        return;
    }

    if (level.intermission_framenum) {
        gi.cprintf(ent, PRINT_HIGH, "You may not initiate votes during intermission.\n");
        return;
    }

    if (level.framenum - level.vote.framenum < 2 * HZ) {
        gi.cprintf(ent, PRINT_HIGH, "You may not initiate votes too soon.\n");
        return;
    }

    if (limit > 0 && ent->client->level.vote.count >= limit) {
        gi.cprintf(ent, PRINT_HIGH, "You may not initiate any more votes.\n");
        return;
    }

    for (v = vote_proposals; v->name; v++) {
        if (!strcmp(s, v->name)) {
            break;
        }
    }
    if (!v->name) {
        gi.cprintf(ent, PRINT_HIGH, "Unknown proposal '%s'. Type '%s help' for usage.\n", s, gi.argv(0));
        return;
    }

    if (!(mask & v->bit)) {
        gi.cprintf(ent, PRINT_HIGH, "Voting for '%s' is not allowed on this server.\n", v->name);
        return;
    }

    if (argc < 3 && v->bit != VOTE_RESET) {
        if (v->bit == VOTE_MAP) {
            map_entry_t *map;
            char buffer[MAX_STRING_CHARS];
            size_t total, len;

            total = 0;
            LIST_FOR_EACH(map_entry_t, map, &g_map_list, list) {
                if (map->flags & MAP_NOVOTE) {
                    continue;
                }
                len = strlen(map->name);
                if (total + len + 2 >= sizeof(buffer)) {
                    break;
                }
                memcpy(buffer + total, map->name, len);
                buffer[total + len    ] = ',';
                buffer[total + len + 1] = ' ';
                total += len + 2;
            }
            if (total > 2) {
                total -= 2;
            }
            buffer[total] = 0;
            gi.cprintf(ent, PRINT_HIGH, "Available maplist: %s\n", buffer);
        } else if (v->bit == VOTE_CONFIG) {
            if (LIST_EMPTY(&configlist)) {
                gi.cprintf(ent, PRINT_HIGH, "No configs are defined\n");
            } else {
                gi.cprintf(ent, PRINT_HIGH, "Available configs:\n");
                localconfig_t *c;
                FOR_EACH_CONFIG(c) {
                    gi.cprintf(ent, PRINT_HIGH, "  %s\n", c->config);
                }
            }
        } else {
            gi.cprintf(ent, PRINT_HIGH, "Argument required for '%s'. Type '%s help' for usage.\n", v->name, gi.argv(0));
        }
        return;
    }

    if (!v->func(ent)) {
        return;
    }

    if (v->bit <= VOTE_MAP) {   // global
        level.vote.initiator = ent->client;
        level.vote.proposal = v->bit;
        level.vote.framenum = level.framenum + g_vote_time->value * HZ;
        level.vote.index++;
    } else {    // per arena
        a->vote.initiator = ent;
        a->vote.proposal = v->bit;
        a->vote.framenum = level.framenum + SECS_TO_FRAMES(g_vote_time->value);
        a->vote.index++;
    }

    G_BuildProposal(buffer, a);

    if (a->vote.proposal) {
        G_bprintf(a, PRINT_HIGH, "%s has initiated a vote: %s\n", NAME(ent), buffer);
        ent->client->level.vote.index = a->vote.index;
    } else {
        gi.bprintf(PRINT_HIGH, "%s has initiated a vote: %s\n", NAME(ent), buffer);
        ent->client->level.vote.index = level.vote.index;
    }

    ent->client->level.vote.accepted = qtrue;
    ent->client->level.vote.count++;

    // decide vote immediately
    if (!G_CheckVote() && !G_CheckArenaVote(a)) {
        if (VF(SHOW)) {
            gi.configstring(CS_VOTE_PROPOSAL, va("Vote: %s", buffer));
            G_UpdateVote();
        }

        if (level.vote.proposal) {
            gi.bprintf(PRINT_MEDIUM, "Type 'yes' in the console to accept or 'no' to deny.\n");
        } else {
            G_bprintf(ARENA(ent), PRINT_MEDIUM, "Type 'yes' in the console to accept or 'no' to deny.\n");
        }
    }
}

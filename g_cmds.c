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
#include "m_player.h"

/**
 *
 */
static void SelectNextItem(edict_t *ent, int itflags)
{
    gclient_t     *cl;
    int           i, index;
    const gitem_t *it;

    cl = ent->client;

    if (cl->layout == LAYOUT_MENU) {
        PMenu_Next(ent);
        return;
    }
    if (cl->chase_target) {
        ChaseNext(ent);
        cl->chase_mode = CHASE_NONE;
        return;
    }

    // scan  for the next valid one
    for (i = 1; i <= ITEM_TOTAL; i++) {
        index = (cl->selected_item + i) % ITEM_TOTAL;
        if (!cl->inventory[index])
            continue;
        it = INDEX_ITEM(index);
        if (!it->use)
            continue;
        if (!(it->flags & itflags))
            continue;

        cl->selected_item = index;
        return;
    }

    cl->selected_item = -1;
}

/**
 *
 */
static void SelectPrevItem(edict_t *ent, int itflags)
{
    gclient_t *cl;
    int i, index;
    const gitem_t *it;

    cl = ent->client;

    if (cl->layout == LAYOUT_MENU) {
        PMenu_Prev(ent);
        return;
    }

    if (cl->chase_target) {
        ChasePrev(ent);
        cl->chase_mode = CHASE_NONE;
        return;
    }

    // scan  for the next valid one
    for (i = 1; i <= ITEM_TOTAL; i++) {
        index = (cl->selected_item + ITEM_TOTAL - i) % ITEM_TOTAL;
        if (!cl->inventory[index])
            continue;
        it = INDEX_ITEM(index);
        if (!it->use)
            continue;
        if (!(it->flags & itflags))
            continue;

        cl->selected_item = index;
        return;
    }

    cl->selected_item = -1;
}

/**
 *
 */
void ValidateSelectedItem(edict_t *ent)
{
    gclient_t   *cl;

    cl = ent->client;

    if (cl->inventory[cl->selected_item]) {
        return;     // valid
    }

    SelectNextItem(ent, -1);
}

/**
 *
 */
static qboolean CheckCheats(edict_t *ent)
{
    if (!PLAYER_SPAWNED(ent)) {
        return qfalse;
    }

    if ((int)sv_cheats->value == 0) {
        gi.cprintf(ent, PRINT_HIGH, "Cheats are disabled on this server.\n");
        return qfalse;
    }

    return qtrue;
}

/**
 * All players have to issue ready command to start the match
 */
static void Cmd_Ready_f(edict_t *ent)
{
    uint8_t i;
    client_persistant_t *p;
    char roundtime[6];

    if (!ent->client)
        return;
    
    if (!TEAM(ent))
        return;
    
    p = &ent->client->pers;
    
    if (ARENA(ent)->current_round > 1) {
        gi.cprintf(ent, PRINT_HIGH, "Can't change ready status mid-match\n");
        return;
    }

    if (!p->ready) {
        p->ready = qtrue;
        G_bprintf(ARENA(ent), PRINT_HIGH, "%s is ready\n", NAME(ent));
        for (i=0; i<MAX_TEAM_PLAYERS; i++) {
            if (!TEAM(ent)->players[i])
                continue;

            if (!TEAM(ent)->players[i]->client->pers.ready) {
                p->team->ready = qfalse;
                ARENA(ent)->ready = qfalse;
                return;
            }
        }

        p->team->ready = qtrue;

    } else {
        p->ready = qfalse;
        p->team->ready = qfalse;
        ARENA(ent)->ready = qfalse;

        if (p->arena->state == ARENA_STATE_COUNTDOWN) {
            p->arena->round_start_frame = 0;
            p->arena->state = ARENA_STATE_WARMUP;
            G_bprintf(ARENA(ent), PRINT_HIGH, "Countdown aborted, ", NAME(ent));
            G_ArenaStuff(ARENA(ent), "stopsound");
            G_SecsToString(roundtime, ARENA(ent)->timelimit);
            G_ConfigString(ARENA(ent), CS_MATCH_STATUS, va("Warmup %s", roundtime));
        }
        
        G_bprintf(ARENA(ent), PRINT_HIGH, "%s is not ready\n", NAME(ent));
        return;
    }

    G_CheckReady(ARENA(ent));
}

/**
 * Console means to changing areans
 */
static void Cmd_Arena_f(edict_t *ent)
{
    arena_t *a;
    
    if (gi.argc() != 2) {
        gi.cprintf(ent, PRINT_HIGH, "Usage: arena <ID>\n\nArena list for %s:\n\n   ID   Cl    Name\n", level.mapname);
        
        FOR_EACH_ARENA(a) {
            if (ARENA(ent) == a) {
                gi.cprintf(ent, PRINT_HIGH, "-> %d    %02d    %s <-\n", 
                    a->number, 
                    a->client_count, 
                    a->name
                );
            } else {
                gi.cprintf(ent, PRINT_HIGH, "   %d    %02d    %s\n", 
                    a->number, 
                    a->client_count, 
                    a->name
                );
            }
        }
        return;
    }
    
    uint8_t newarena = atoi(gi.argv(1));
    clamp(newarena, 1, level.arena_count);
    
    G_ChangeArena(ent, &level.arenas[newarena]);
}

/**
 * Console means of joining teams
 */
static void Cmd_Team_f(edict_t *ent)
{
    if (gi.argc() != 2) {
        gi.cprintf(ent, PRINT_HIGH, "Usage: \"team #\" to join a team (1-%d)\n", ARENA(ent)->team_count);
        return;
    }
    
    char *team= gi.argv(1);

    if (str_equal(team, "1")) {
        G_TeamJoin(ent, TEAM_RED, false);
        return;
    }
    
    if (str_equal(team, "2")) {
        G_TeamJoin(ent, TEAM_BLUE, false);
        return;
    }

    if (str_equal(team, "3") && ARENA(ent)->team_count >= 3) {
        G_TeamJoin(ent, TEAM_GREEN, false);
        return;
    }
    
    if (str_equal(team, "4") && ARENA(ent)->team_count >= 4) {
        G_TeamJoin(ent, TEAM_YELLOW, false);
        return;
    }

    if (str_equal(team, "5") && ARENA(ent)->team_count == 5) {
        G_TeamJoin(ent, TEAM_AQUA, false);
        return;
    }

    gi.cprintf(ent, PRINT_HIGH, "Unknown team, use \"team 1-%d\"\n", ARENA(ent)->team_count);
}

/**
 * Give items to a client
 */
static void Cmd_Give_f(edict_t *ent)
{
    char        *name;
    gitem_t     *it;
    int         index;
    int         i;
    qboolean    give_all;
    edict_t     *it_ent;

    if (!CheckCheats(ent)) {
        return;
    }

    name = gi.args();

    if (Q_stricmp(name, "all") == 0)
        give_all = qtrue;
    else
        give_all = qfalse;

    if (give_all || Q_stricmp(gi.argv(1), "health") == 0) {
        if (gi.argc() == 3)
            ent->health = atoi(gi.argv(2));
        else
            ent->health = ent->max_health;
        if (!give_all)
            return;
    }

    if (give_all || Q_stricmp(name, "weapons") == 0) {
        for (i = 0; i < ITEM_TOTAL; i++) {
            it = INDEX_ITEM(i);
            if (!it->pickup)
                continue;
            if (!(it->flags & IT_WEAPON))
                continue;
            ent->client->inventory[i] += 1;
        }
        if (!give_all)
            return;
    }

    if (give_all || Q_stricmp(name, "ammo") == 0) {
        for (i = 0; i < ITEM_TOTAL; i++) {
            it = INDEX_ITEM(i);
            if (!it->pickup)
                continue;
            if (!(it->flags & IT_AMMO))
                continue;
            Add_Ammo(ent, it, 1000);
        }
        if (!give_all)
            return;
    }

    if (give_all || Q_stricmp(name, "armor") == 0) {
        gitem_armor_t   *info;

        ent->client->inventory[ITEM_ARMOR_JACKET] = 0;

        ent->client->inventory[ITEM_ARMOR_COMBAT] = 0;

        it = INDEX_ITEM(ITEM_ARMOR_BODY);
        info = (gitem_armor_t *)it->info;
        ent->client->inventory[ITEM_ARMOR_BODY] = info->max_count;

        if (!give_all)
            return;
    }

    if (give_all || Q_stricmp(name, "Power Shield") == 0) {
        it = INDEX_ITEM(ITEM_POWER_SHIELD);
        it_ent = G_Spawn();
        it_ent->classname = it->classname;
        SpawnItem(it_ent, it);
        if (it_ent->inuse) {
            Touch_Item(it_ent, ent, NULL, NULL);
            if (it_ent->inuse)
                G_FreeEdict(it_ent);
        }

        if (!give_all)
            return;
    }

    if (give_all) {
        for (i = 0; i < ITEM_TOTAL; i++) {
            it = INDEX_ITEM(i);
            if (!it->pickup)
                continue;
            if (it->flags & (IT_ARMOR | IT_WEAPON | IT_AMMO))
                continue;
            ent->client->inventory[i] = 1;
        }
        return;
    }

    it = FindItem(name);
    if (!it) {
        name = gi.argv(1);
        it = FindItem(name);
        if (!it) {
            gi.cprintf(ent, PRINT_HIGH, "unknown item\n");
            return;
        }
    }

    if (!it->pickup) {
        gi.cprintf(ent, PRINT_HIGH, "non-pickup item\n");
        return;
    }

    index = ITEM_INDEX(it);

    if (it->flags & IT_AMMO) {
        if (gi.argc() == 3)
            ent->client->inventory[index] = atoi(gi.argv(2));
        else
            ent->client->inventory[index] += it->quantity;
    } else {
        it_ent = G_Spawn();
        it_ent->classname = it->classname;
        SpawnItem(it_ent, it);
        if (it_ent->inuse) {
            Touch_Item(it_ent, ent, NULL, NULL);
            if (it_ent->inuse)
                G_FreeEdict(it_ent);
        }
    }
}


/**
 * Enables god mode for client
 */
static void Cmd_God_f(edict_t *ent)
{
    if (CheckCheats(ent)) {
        ent->flags ^= FL_GODMODE;
        gi.cprintf(ent, PRINT_HIGH, "godmode %s\n",
                (ent->flags & FL_GODMODE) ? "ON" : "OFF"
        );
    }
}


/**
 * Set's cilent to notarget
 */
static void Cmd_Notarget_f(edict_t *ent)
{
    if (CheckCheats(ent)) {
        ent->flags ^= FL_NOTARGET;
        gi.cprintf(ent, PRINT_HIGH, "notarget %s\n",
                (ent->flags & FL_NOTARGET) ? "ON" : "OFF");
    }
}


/**
 * Set's client to noclip
 */
static void Cmd_Noclip_f(edict_t *ent)
{
    if (!CheckCheats(ent)) {
        return;
    }

    if (ent->movetype == MOVETYPE_NOCLIP) {
        ent->movetype = MOVETYPE_WALK;
    } else {
        ent->movetype = MOVETYPE_NOCLIP;
    }

    gi.cprintf(ent, PRINT_HIGH, "noclip %s\n",
               ent->movetype == MOVETYPE_NOCLIP ? "ON" : "OFF");
}

/**
 * Call a timeout, all players freeze.
 *
 * Normally we'd want to stop the regular game clock for a timeout, but
 * G_ArenaThink() returns after G_TimeoutFrame() runs, so the clock's tick
 * function pointer won't get called anyway.
 */
static void Cmd_Timeout_f(edict_t *ent) {
    arena_t *a = ARENA(ent);

    if (!TEAM(ent)) {
        return;
    }

    // handle timein
    if (a->state == ARENA_STATE_TIMEOUT) {
        if (ent == a->timeout_caller || ent->client->pers.admin) {
            G_bprintf(a, PRINT_HIGH, "%s canceled the timeout\n", NAME(ent));
            if (a->timeout_clock.value > (int)g_timein_time->value) {
                a->timeout_clock.value = (int)g_timein_time->value;
            }
        } 
        return;
    }

    if (a->state != ARENA_STATE_PLAY) {
        return;
    }

    a->state = ARENA_STATE_TIMEOUT;
    a->timeout_caller = ent;

    G_bprintf(a, PRINT_HIGH, "%s called timeout\n", NAME(ent));
    G_ArenaSound(a, level.sounds.timeout);
    ClockStartTimeout(a);
}

/**
 * Use an inventory item
 */
static void Cmd_Use_f(edict_t *ent)
{
    int         index;
    gitem_t     *it;
    char        *s;

    s = gi.args();
    it = FindItem(s);
    if (!it) {
        gi.cprintf(ent, PRINT_HIGH, "Unknown item: %s\n", s);
        return;
    }

    if (!it->use) {
        gi.cprintf(ent, PRINT_HIGH, "Item is not usable.\n");
        return;
    }

    index = ITEM_INDEX(it);
    if (!ent->client->inventory[index]) {
        gi.cprintf(ent, PRINT_HIGH, "Out of item: %s\n", s);
        return;
    }

    it->use(ent, it);
}


/**
 * Drop an item
 */
static void Cmd_Drop_f(edict_t *ent)
{
    int         index;
    gitem_t     *it;
    char        *s;

    if (!g_drop_allowed->value) {
        gi.cprintf(ent, PRINT_HIGH, "Dropping items is disabled.\n");
        return;
    }

    s = gi.args();
    it = FindItem(s);
    if (!it) {
        gi.cprintf(ent, PRINT_HIGH, "Unknown item: %s\n", s);
        return;
    }

    if (!it->drop) {
        gi.cprintf(ent, PRINT_HIGH, "Item is not dropable.\n");
        return;
    }

    index = ITEM_INDEX(it);
    if (!ent->client->inventory[index]) {
        gi.cprintf(ent, PRINT_HIGH, "Out of item: %s\n", s);
        return;
    }

    it->drop(ent, it);
}


/**
 *
 */
static void Cmd_Inven_f(edict_t *ent)
{
    // this one is left for backwards compatibility
    Cmd_Menu_f(ent);
}

/**
 *
 */
static void Cmd_InvUse_f(edict_t *ent)
{
    gitem_t     *it;

    if (ent->client->layout == LAYOUT_MENU) {
        PMenu_Select(ent);
        return;
    }

    ValidateSelectedItem(ent);

    if (ent->client->selected_item == -1) {
        gi.cprintf(ent, PRINT_HIGH, "No item to use.\n");
        return;
    }

    it = INDEX_ITEM(ent->client->selected_item);
    if (!it->use) {
        gi.cprintf(ent, PRINT_HIGH, "Item is not usable.\n");
        return;
    }
    it->use(ent, it);
}

/**
 *
 */
static void Cmd_WeapPrev_f(edict_t *ent)
{
    gclient_t   *cl;
    int         i, index;
    gitem_t     *it;
    int         selected_weapon;

    cl = ent->client;

    if (!cl->weapon)
        return;

    selected_weapon = ITEM_INDEX(cl->weapon);

    // scan  for the next valid one
    for (i = 1; i <= ITEM_TOTAL; i++) {
        index = (selected_weapon + i) % ITEM_TOTAL;
        if (!cl->inventory[index]) {
            continue;
        }

        it = INDEX_ITEM(index);
        if (!it->use) {
            continue;
        }

        if (!(it->flags & IT_WEAPON)) {
            continue;
        }

        it->use(ent, it);

        if (cl->weapon == it) {
            return; // successful
        }
    }
}

/**
 *
 */
static void Cmd_WeapNext_f(edict_t *ent)
{
    gclient_t   *cl;
    int         i, index;
    gitem_t     *it;
    int         selected_weapon;

    cl = ent->client;

    if (!cl->weapon)
        return;

    selected_weapon = ITEM_INDEX(cl->weapon);

    // scan  for the next valid one
    for (i = 1; i <= ITEM_TOTAL; i++) {
        index = (selected_weapon + ITEM_TOTAL - i) % ITEM_TOTAL;
        if (!cl->inventory[index]) {
            continue;
        }

        it = INDEX_ITEM(index);
        if (!it->use) {
            continue;
        }

        if (!(it->flags & IT_WEAPON)) {
            continue;
        }

        it->use(ent, it);

        if (cl->weapon == it) {
            return; // successful
        }
    }
}

/**
 *
 */
static void Cmd_WeapLast_f(edict_t *ent)
{
    gclient_t   *cl;
    int         index;
    gitem_t     *it;

    cl = ent->client;

    if (!cl->weapon || !cl->lastweapon)
        return;

    index = ITEM_INDEX(cl->lastweapon);
    if (!cl->inventory[index]) {
        return;
    }

    it = INDEX_ITEM(index);
    if (!it->use) {
        return;
    }

    if (!(it->flags & IT_WEAPON)) {
        return;
    }

    it->use(ent, it);
}

/**
 *
 */
static void Cmd_InvDrop_f(edict_t *ent)
{
    gitem_t     *it;

    if (ent->client->layout == LAYOUT_MENU) {
        PMenu_Select(ent);
        return;
    }

    ValidateSelectedItem(ent);

    if (ent->client->selected_item == -1) {
        gi.cprintf(ent, PRINT_HIGH, "No item to drop.\n");
        return;
    }

    it = INDEX_ITEM(ent->client->selected_item);
    if (!it->drop) {
        gi.cprintf(ent, PRINT_HIGH, "Item is not dropable.\n");
        return;
    }

    it->drop(ent, it);
}

/**
 * Go kill yourself
 */
static void Cmd_Kill_f(edict_t *ent)
{
    if (!PLAYER_SPAWNED(ent))
        return;

    if (level.framenum - ent->client->respawn_framenum < 5 * HZ)
        return;

    ent->flags &= ~FL_GODMODE;
    ent->health = 0;
    meansOfDeath = MOD_SUICIDE;

    player_die(ent, ent, ent, 100000, vec3_origin);
}

/**
 *
 */
static void Cmd_PutAway_f(edict_t *ent)
{
    ent->client->layout = 0;
}

/**
 *
 */
qboolean G_FloodProtect(edict_t *ent, flood_t *flood,
                        const char *what, int msgs, float persecond, float delay)
{
    int i, sec;

    if (msgs < 1) {
        return qfalse;
    }

    if (level.framenum < flood->locktill) {
        sec = (flood->locktill - level.framenum) / HZ;
        if (sec < 1) {
            sec = 1;
        }

        gi.cprintf(ent, PRINT_HIGH, "You can't %s for %d more second%s.\n",
                   what, sec, sec == 1 ? "" : "s");
        return qtrue;
    }

    i = flood->whenhead - msgs + 1;
    if (i >= 0 && level.framenum - flood->when[i % FLOOD_MSGS] < persecond * HZ) {
        flood->locktill = level.framenum + delay * HZ;
        gi.cprintf(ent, PRINT_CHAT,
                   "Flood protection: You can't %s for %d seconds.\n", what, (int)delay);
        gi.dprintf("%s can't %s for %d seconds\n",
                   ent->client->pers.netname, what, (int)delay);
        return qtrue;
    }

    flood->when[++flood->whenhead % FLOOD_MSGS] = level.framenum;
    return qfalse;
}


/**
 *
 */
static void Cmd_Wave_f(edict_t *ent)
{
    int     i;

    // spectators can't wave!
    if (!PLAYER_SPAWNED(ent)) {
        return;
    }

    // can't wave when ducked
    if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
        return;

    if (ent->client->anim_priority > ANIM_WAVE)
        return;

    if (G_FloodProtect(ent, &ent->client->level.wave_flood, "use waves",
                       (int)flood_waves->value, flood_perwave->value, flood_wavedelay->value)) {
        return;
    }

    i = atoi(gi.argv(1));

    ent->client->anim_priority = ANIM_WAVE;

    switch (i) {
    case 0:
        gi.cprintf(ent, PRINT_LOW, "flipoff\n");
        ent->client->anim_start = FRAME_flip01;
        ent->client->anim_end = FRAME_flip12;
        break;
    case 1:
        gi.cprintf(ent, PRINT_LOW, "salute\n");
        ent->client->anim_start = FRAME_salute01;
        ent->client->anim_end = FRAME_salute11;
        break;
    case 2:
        gi.cprintf(ent, PRINT_LOW, "taunt\n");
        ent->client->anim_start = FRAME_taunt01;
        ent->client->anim_end = FRAME_taunt17;
        break;
    case 3:
        gi.cprintf(ent, PRINT_LOW, "wave\n");
        ent->client->anim_start = FRAME_wave01;
        ent->client->anim_end = FRAME_wave11;
        break;
    case 4:
    default:
        gi.cprintf(ent, PRINT_LOW, "point\n");
        ent->client->anim_start = FRAME_point01;
        ent->client->anim_end = FRAME_point12;
        break;
    }
}

#define MAX_CHAT   150

typedef enum {
    CHAT_MISC,
    CHAT_ALL,   // send to everyone on server`
    CHAT_ARENA, // only people in sender's arena
    CHAT_TEAM,  // only members of sender's team
} chat_t;

/**
 *
 */
static size_t build_chat(edict_t *ent, chat_t chat, int start, char *buffer)
{
    size_t len, total;
    int i;
    char *p;
    char *name = ent->client->pers.netname;
    int arena = ent->client->pers.arena->number;

    switch (chat) {
        case CHAT_TEAM:
            total = Q_scnprintf(buffer, MAX_CHAT, "(%s): ", name);
            break;
        case CHAT_ALL:
            total = Q_scnprintf(buffer, MAX_CHAT, "%s (arena %d): ", name, arena);
            break;
        default:
            total = Q_scnprintf(buffer, MAX_CHAT, "%s: ", name);
    }
    
    for (i = start; i < gi.argc(); i++) {
        p = gi.argv(i);
        len = strlen(p);
        if (!len)
            continue;

        if (total + len + 1 >= MAX_CHAT)
            break;

        while (*p) {
            int c = *p++;
            c &= 127;   // strip high bits

            // don't allow carriage returns, etc
            if (!Q_isspecial(c))
                buffer[total++] = c;
        }

        buffer[total++] = ' ';
    }

    buffer[total] = 0;

    return total;
}

/**
 *
 */
static void Cmd_Say_f(edict_t *ent, chat_t chat)
{
    int8_t        i, start;
    edict_t        *other;
    char        text[MAX_CHAT];
    gclient_t    *cl = ent->client;

    if (cl->pers.muted) {
        gi.cprintf(ent, PRINT_HIGH, "You are not allowed to talk.\n");
        return;
    }
    
    if (chat == CHAT_ALL && !(int) g_all_chat->value) {
        gi.cprintf(ent, PRINT_HIGH, "%s command is disabled\n", gi.argv(0));
        return;
    }
        
    start = (chat == CHAT_MISC) ? 0 : 1;
    
    // default is arena chat
    if (!start) {
        chat = CHAT_ARENA;
    }
    
    if (gi.argc() <= start)
        return;

    // stop flood during the match
    if (!cl->pers.admin && ARENA(ent)->state == ARENA_STATE_PLAY) {
        
        if (g_mute_chat->value) {
            if (TEAM(ent)) {
                gi.cprintf(ent, PRINT_HIGH, "Players can't talk during the match.\n");
                return;
            }
            if ((int)g_mute_chat->value > 1) {
                gi.cprintf(ent, PRINT_HIGH, "Spectators can't talk during the match.\n");
                return;
            }
            chat = CHAT_TEAM;
        }
    }

    if (G_FloodProtect(ent, &cl->level.chat_flood, "talk",
                       (int)flood_msgs->value, flood_persecond->value, flood_waitdelay->value)) {
        return;
    }

    build_chat(ent, chat, start, text);

    if ((int)dedicated->value) {
        gi.cprintf(NULL, PRINT_CHAT, "%s\n", text);
    }

    for (i = 1; i <= game.maxclients; i++) {
        other = &g_edicts[i];
        if (!other->inuse)
            continue;
        
        if (!other->client)
            continue;
        
        if (chat == CHAT_TEAM && !G_Teammates(ent, other))
            continue;
        
        // for spectators
        if (chat == CHAT_TEAM && ARENA(ent) != ARENA(other))
            continue;
        
        if (chat == CHAT_ARENA && ARENA(other) != ARENA(ent))
            continue;
        
        gi.cprintf(other, PRINT_CHAT, "%s\n", text);
    }
}

/**
 * List the currently connected players
 */
void Cmd_Players_f(edict_t *ent)
{
    arena_t *arena;
    arena_team_t *team;
    gclient_t *c;
    int i, sec;
    char score[16], idle[16], time[16];
    qboolean show_ips = !ent || ent->client->pers.admin;

    gi.cprintf(ent, PRINT_HIGH,
               "id score ping time name            idle arena team            capt%s\n"
               "-- ----- ---- ---- --------------- ---- ----- --------------- ----%s\n",
               show_ips ? " address               " : "",
               show_ips ? " ----------------------" : "");

    for (i = 0; i < game.maxclients; i++) {
        c = &game.clients[i];
        if (c->pers.connected <= CONN_CONNECTED) {
            continue;
        }

        if (c->pers.connected == CONN_SPAWNED) {
            sprintf(score, "%d", c->resp.score);
            sec = (level.framenum - c->resp.activity_framenum) / HZ;
            if (level.framenum < 10 * 60 * HZ) {
                sprintf(idle, "%d:%02d", sec / 60, sec % 60);
            } else {
                sprintf(idle, "%d", sec / 60);
            }
        } else {
            strcpy(score, "SPECT");
            strcpy(idle, "-");
        }

        sec = (level.framenum - c->resp.enter_framenum) / HZ;
        if (level.framenum < 10 * 60 * HZ) {
            sprintf(time, "%d:%02d", sec / 60, sec % 60);
        } else {
            sprintf(time, "%d", sec / 60);
        }
        
        arena = ARENA(c->edict);
        team = TEAM(c->edict);
        
        char *teamname = "";
        char *capt = "";
        
        if (team) {
            teamname = team->name;
            
            if (team->captain == c->edict) {
                capt = "*";
            }
        }
        
        gi.cprintf(ent, PRINT_HIGH, "%2d %5s %4d %4s %-15s %4s %5s %-15s %4s %4s\n",
                   i, score, c->ping, time, c->pers.netname, idle,
                   va("%d", arena->number), teamname, capt,
                   show_ips ? IP(&c->pers.addr) : ""
        );
    }
}

/**
 *
 */
void Cmd_HighScores_f(edict_t *ent)
{
    int i;
    char date[MAX_QPATH];
    struct tm *tm;
    score_t *s;
    size_t len;

    if (!level.numscores) {
        gi.cprintf(ent, PRINT_HIGH, "No high scores available.\n");
        return;
    }

    gi.cprintf(ent, PRINT_HIGH,
               " # Name            FPH  Date\n"
               "-- --------------- ---- ----------------\n");
    for (i = 0; i < level.numscores; i++) {
        s = &level.scores[i];

        tm = localtime(&s->time);
        len = strftime(date, sizeof(date), "%Y-%m-%d %H:%M", tm);
        if (len < 1)
            strcpy(date, "???");
        gi.cprintf(ent, PRINT_HIGH, "%2d %-15.15s %4d %s\n",
                   i + 1, s->name, s->score, date);
    }
}

/**
 *
 */
edict_t *G_SetPlayer(edict_t *ent, int arg)
{
    edict_t     *other, *match;
    int         i, count;
    char        *s;

    s = gi.argv(arg);

    // numeric values are just slot numbers
    if (COM_IsUint(s)) {
        i = atoi(s);
        if (i < 0 || i >= game.maxclients) {
            gi.cprintf(ent, PRINT_HIGH, "Bad client slot number: %d\n", i);
            return NULL;
        }

        other = &g_edicts[ i + 1 ];
        if (!ValidChaseTarget(ent, other)) {
            gi.cprintf(ent, PRINT_HIGH, "You're unable to chase %s now\n", NAME(ent));
            return NULL;
        }
        return other;
    }

    // check for a name match
    match = NULL;
    count = 0;
    for (i = 0; i < game.maxclients; i++) {
        other = &g_edicts[ i + 1 ];
        if (!other->client) {
            continue;
        }

        if (!Q_stricmp(NAME(other), s)) {
            if (!ValidChaseTarget(ent, other)) {
                continue;
            }
            return other; // exact match
        }

        if (Q_stristr(NAME(other), s)) {
            if (!ValidChaseTarget(ent, other)) {
                continue;
            }
            match = other; // partial match
            count++;
        }
    }

    if (!match) {
        gi.cprintf(ent, PRINT_HIGH, "No clients matching '%s' found.\n", s);
        return NULL;
    }

    if (count > 1) {
        gi.cprintf(ent, PRINT_HIGH, "'%s' matches multiple clients.\n", s);
        return NULL;
    }

    return match;
}

/**
 *
 */
edict_t *G_SetVictim(edict_t *ent, int start)
{
    edict_t *other = G_SetPlayer(ent, start + 1);

    if (!other) {
        return NULL;
    }

    if (other == ent) {
        gi.cprintf(ent, PRINT_HIGH, "You can't %s yourself.\n", gi.argv(start));
        return NULL;
    }

    if (other->client->pers.loopback || other->client->pers.mvdspec) {
        gi.cprintf(ent, PRINT_HIGH, "You can't %s local client.\n", gi.argv(start));
        return NULL;
    }

    if (other->client->pers.admin) {
        gi.cprintf(ent, PRINT_HIGH, "You can't %s an admin.\n", gi.argv(start));
        return NULL;
    }

    return other;
}

/**
 *
 */
static qboolean G_SpecRateLimited(edict_t *ent)
{
    if (level.framenum - ent->client->resp.enter_framenum < 5 * HZ) {
        gi.cprintf(ent, PRINT_HIGH, "You may not change modes too soon.\n");
        return qtrue;
    }

    return qfalse;
}

/**
 *
 */
static void Cmd_Observe_f(edict_t *ent)
{
    G_TeamPart(ent, false);
    G_SpectatorsJoin(ent);
    
    if (ent->client->pers.connected == CONN_PREGAME) {
        ent->client->pers.connected = CONN_SPECTATOR;
        gi.cprintf(ent, PRINT_HIGH, "Changed to spectator mode.\n");
        return;
    }
}

/**
 *
 */
static void Cmd_Chase_f(edict_t *ent)
{
    edict_t *target = NULL;
    chase_mode_t mode = CHASE_NONE;

    // they're currently on a team, remove them
    if (TEAM(ent) != NULL) {
        G_TeamPart(ent, false);
    }

    if (gi.argc() == 2) {
        char *who = gi.argv(1);

        if (!Q_stricmp(who, "top") || !Q_stricmp(who, "leader")) {
            mode = CHASE_LEADER;
        } else {
            target = G_SetPlayer(ent, 1);
            if (!target) {
                return;
            }

            if (!IS_PLAYER(target)) {
                gi.cprintf(ent, PRINT_HIGH, "'%s' is a spectator\n", NAME(target));
                return;
            }
        }
    }

    // changing from pregame mode into spectator
    if (ent->client->pers.connected == CONN_PREGAME) {
        ent->client->pers.connected = CONN_SPECTATOR;
        gi.cprintf(ent, PRINT_HIGH, "Changed to spectator mode.\n");
        if (target) {
            SetChaseTarget(ent, target);
            ent->client->chase_mode = CHASE_NONE;
        } else {
            GetChaseTarget(ent, mode);
        }
        return;
    }

    // respawn the spectator
    if (ent->client->pers.connected != CONN_SPECTATOR) {
        if (G_SpecRateLimited(ent)) {
            return;
        }
        spectator_respawn(ent, CONN_SPECTATOR);
    }

    if (target) {
        if (target == ent->client->chase_target) {
            gi.cprintf(ent, PRINT_HIGH,
                       "You are already chasing this player.\n");
            return;
        }
        SetChaseTarget(ent, target);
        ent->client->chase_mode = CHASE_NONE;
    } else {
        if (!ent->client->chase_target || mode != CHASE_NONE) {
            GetChaseTarget(ent, mode);
        } else {
            SetChaseTarget(ent, NULL);
        }
    }
}

static const char weapnames[WEAP_TOTAL][12] = {
    "None",         "Blaster",      "Shotgun",      "S.Shotgun",
    "Machinegun",   "Chaingun",     "Grenades",     "G.Launcher",
    "R.Launcher",   "H.Blaster",    "Railgun",      "BFG10K"
};


// for testing sounds, remove later
static void Cmd_Sound_f(edict_t *ent)
{
    
    // ex: world/10_0.wav
    int index = gi.soundindex(gi.args());
    if (!index) {
        gi.cprintf(ent, PRINT_HIGH, "Unknown Sound: %s\n", gi.args());
        return;
    }
    
    G_ArenaSound(ARENA(ent), index);
}

/**
 *
 */
void Cmd_Stats_f(edict_t *ent, qboolean check_other)
{
    int i;
    fragstat_t *s;
    char acc[16];
    char hits[16];
    char frgs[16];
    char dths[16];
    edict_t *other;

    if (!ent) {
        if (gi.argc() < 3) {
            gi.cprintf(ent, PRINT_HIGH, "Usage: %s <playerID>\n", gi.argv(1));
            return;
        }
        other = G_SetPlayer(ent, 2);
        if (!other) {
            return;
        }
    } else if (check_other && gi.argc() > 1) {
        other = G_SetPlayer(ent, 1);
        if (!other) {
            return;
        }
    } else if (ent->client->chase_target) {
        other = ent->client->chase_target;
    } else {
        other = ent;
    }

    for (i = FRAG_BLASTER; i <= FRAG_BFG; i++) {
        s = &other->client->resp.frags[i];
        if (s->atts || s->deaths) {
            break;
        }
    }

    if (i > FRAG_BFG) {
        gi.cprintf(ent, PRINT_HIGH, "No accuracy stats available for %s.\n",
                   other->client->pers.netname);
        return;
    }

    gi.cprintf(ent, PRINT_HIGH,
               "Accuracy stats for %s:\n"
               "Weapon     Acc%% Hits/Atts Frgs Dths\n"
               "---------- ---- --------- ---- ----\n",
               other->client->pers.netname);

    for (i = FRAG_BLASTER; i <= FRAG_BFG; i++) {
        s = &other->client->resp.frags[i];
        if (!s->atts && !s->deaths) {
            continue;
        }
        if (s->atts && i != FRAG_BFG) {
            sprintf(acc, "%3i%%", s->hits * 100 / s->atts);
            sprintf(hits, "%4d/%-4d", s->hits, s->atts);
        } else {
            strcpy(acc, "    ");
            strcpy(hits, "         ");
        }

        if (s->kills) {
            sprintf(frgs, "%4d", s->kills);
        } else {
            strcpy(frgs, "    ");
        }

        if (s->deaths) {
            sprintf(dths, "%4d", s->deaths);
        } else {
            strcpy(dths, "    ");
        }

        gi.cprintf(ent, PRINT_HIGH,
                   "%-10s %s %s %s %s\n",
                   weapnames[i], acc, hits, frgs, dths);
    }

    gi.cprintf(ent, PRINT_HIGH,
               "Total damage given/recvd: %d/%d\n",
               other->client->resp.damage_given,
               other->client->resp.damage_recvd);
}

/**
 *
 */
static void Cmd_Id_f(edict_t *ent)
{
    ent->client->pers.noviewid ^= 1;

    gi.cprintf(ent, PRINT_HIGH,
               "Player identification display is now %sabled.\n",
               ent->client->pers.noviewid ? "dis" : "en");
}

/**
 * Display arena settings
 */
void Cmd_Settings_f(edict_t *ent)
{
    gi.cprintf(ent, PRINT_HIGH, "Weapons:       %s\n", G_WeaponFlagsToString(ARENA(ent)));
    gi.cprintf(ent, PRINT_HIGH, "Damage:        %s\n", G_DamageFlagsToString(ARENA(ent)->damage_flags));
    gi.cprintf(ent, PRINT_HIGH, "Rounds:        %d\n", ARENA(ent)->round_limit);
    gi.cprintf(ent, PRINT_HIGH, "Teams:         %d\n", ARENA(ent)->team_count);
    gi.cprintf(ent, PRINT_HIGH, "Health:        %d\n", ARENA(ent)->health);
    gi.cprintf(ent, PRINT_HIGH, "Body Armor:    %d\n", ARENA(ent)->armor);
    gi.cprintf(ent, PRINT_HIGH, "Timelimit:     %d\n", ARENA(ent)->timelimit);
    gi.cprintf(ent, PRINT_HIGH, "Fast Switch:   %s\n", (ARENA(ent)->fastswitch) ? "on" : "off");
    gi.cprintf(ent, PRINT_HIGH, "Mode:          %s\n", G_ArenaModeString(ARENA(ent)));
    gi.cprintf(ent, PRINT_HIGH, "Corpse View:   %s\n", (ARENA(ent)->corpseview) ? "on" : "off");
}

/**
 * Elevate your status
 */
static void Cmd_Admin_f(edict_t *ent)
{
    char *p;

    if (!g_admin_password->string[0]) {
        gi.cprintf(ent, PRINT_HIGH, "admin is disabled on this server\n");
        return;
    }
    
    if (ent->client->pers.admin) {
        gi.bprintf(PRINT_HIGH, "%s is no longer an admin.\n",
                   ent->client->pers.netname);
        ent->client->pers.admin = qfalse;
        return;
    }

    if (gi.argc() < 2) {
        gi.cprintf(ent, PRINT_HIGH, "Usage: %s <password>\n", gi.argv(0));
        return;
    }

    p = gi.argv(1);
    if (strcmp(g_admin_password->string, p)) {
        gi.cprintf(ent, PRINT_HIGH, "Bad admin password.\n");
        if ((int)dedicated->value) {
            gi.dprintf("%s[%s] failed to become an admin.\n",
                       ent->client->pers.netname, IP(&ent->client->pers.addr));
        }
        return;
    }

    ent->client->pers.admin = qtrue;
    gi.bprintf(PRINT_HIGH, "%s is now an admin.\n", ent->client->pers.netname);

    G_CheckVote();
    G_CheckArenaVote(ARENA(ent));
}

/**
 * Stop a player from talking
 */
static void Cmd_Mute_f(edict_t *ent, qboolean muted)
{
    edict_t *other;

    if (gi.argc() < 2) {
        gi.cprintf(ent, PRINT_HIGH, "Usage: %s <playerID>\n", gi.argv(0));
        return;
    }

    other = G_SetVictim(ent, 0);
    if (!other) {
        return;
    }

    if (other->client->pers.muted == muted) {
        gi.cprintf(ent, PRINT_HIGH, "%s is already %smuted\n",
                   other->client->pers.netname, muted ? "" : "un");
        return;
    }

    other->client->pers.muted = muted;
    gi.bprintf(PRINT_HIGH, "%s has been %smuted.\n",
               other->client->pers.netname, muted ? "" : "un");
}

/**
 *
 */
static void Cmd_MuteAll_f(edict_t *ent, qboolean muted)
{
    if (!!(int)g_mute_chat->value == muted) {
        gi.cprintf(ent, PRINT_HIGH, "Players are already %smuted\n",
                   muted ? "" : "un");
        return;
    }

    gi.cvar_set("g_mute_chat", muted ? "1" : "0");
    gi.bprintf(PRINT_HIGH, "Players may %s talk during the match.\n",
               muted ? "no longer" : "now");
}

/**
 *
 */
static void Cmd_Kick_f(edict_t *ent, qboolean ban)
{
    edict_t *other;

    if (gi.argc() < 2) {
        gi.cprintf(ent, PRINT_HIGH, "Usage: %s <playerID>\n", gi.argv(0));
        return;
    }

    other = G_SetVictim(ent, 0);
    if (!other) {
        return;
    }

    gi.AddCommandString(va("kick %d\n", (int)(other->client - game.clients)));

    if (ban) {
        G_BanEdict(other, ent);
    }
}

/**
 * List admin commands
 */
static void Cmd_AdminCommands_f(edict_t *ent)
{
    gi.cprintf(ent, PRINT_HIGH,
               "(un)mute    Allow/disallow specific player to talk\n"
               "(un)muteall Allow/disallow everyone to talk\n"
               "(un)ban     Add/remove temporary bans\n"
               "bans        List bans\n"
               "kick        Kick a player\n"
               "kickban     Kick a player and ban him for 1 hour\n"
              );
}

/**
 * List commands
 */
static void Cmd_Commands_f(edict_t *ent)
{
    gi.cprintf(ent, PRINT_HIGH,
               "menu                Show the main menu\n"
               "arena            List available arenas or change your current arena\n"
               "join|team        Join a team\n"
               "observe|obs      Leave your team\n"
               "ready            Ready-up to start the match\n"
               "chase            Enter chasecam mode\n"
               "settings         Show arena settings\n"
               "oldscore         Show previous scoreboard\n"
               "vote             Propose new settings\n"
               "stats            Show accuracy stats\n"
               "players          Show players on server and what arenas they're in\n"
               "highscores       Show the best results on map\n"
               "id               Toggle player ID display\n"
              );
}

/**
 *
 */
static void select_arena(edict_t *ent)
{
    switch (ent->client->menu.cur) {
        case 12:
            PMenu_Close(ent);
            break;
        
    }
    
    int selected = ent->client->menu.cur;
    
    if (selected >= 2 && selected < 12) {
        G_ChangeArena(ent, &level.arenas[selected-1]);
    }
}

/**
 *
 */
static void select_team(edict_t *ent)
{
    switch (ent->client->menu.cur) {
    case 3:
        Cmd_ArenaMenu_f(ent);
        break;
    case 5:
        G_TeamJoin(ent, TEAM_RED, false);
        PMenu_Close(ent);
        break;
    case 6:
        G_TeamJoin(ent, TEAM_BLUE, false);
        PMenu_Close(ent);
        break;
    case 7:
        G_TeamJoin(ent, TEAM_GREEN, false);
        PMenu_Close(ent);
        break;
    case 8:
        G_TeamJoin(ent, TEAM_YELLOW, false);
        PMenu_Close(ent);
        break;
    case 9:
        G_TeamJoin(ent, TEAM_AQUA, false);
        PMenu_Close(ent);
        break;
    case 11:
        PMenu_Close(ent);
        break;
    }
}

static const pmenu_entry_t main_menu[MAX_MENU_ENTRIES] = {
    { "Main Main", PMENU_ALIGN_CENTER },
    { NULL },
    { NULL },
    { "*Change ARENA", PMENU_ALIGN_LEFT, select_team },
    { NULL },
    { NULL },
    { NULL },
    { NULL },
    { NULL },
    { NULL },
    { NULL },
    { "*Exit menu", PMENU_ALIGN_LEFT, select_team },
    { NULL },
    { "Use [ and ] to move cursor", PMENU_ALIGN_CENTER },
    { "Press Enter to select", PMENU_ALIGN_CENTER },
    { NULL },
    { NULL },
    { "*OpenRA2 " OPENRA2_VERSION, PMENU_ALIGN_RIGHT }
};

static const pmenu_entry_t arena_menu[MAX_MENU_ENTRIES] = {
    { "Select Arena", PMENU_ALIGN_CENTER },
    { NULL },
    { NULL },
    { NULL },
    { NULL },
    { NULL },
    { NULL },    
    { NULL },
    { NULL },
    { NULL },
    { NULL },
    { NULL },
    { "*Exit menu", PMENU_ALIGN_LEFT, select_arena },
    { NULL },
    { NULL },
    { NULL },
    { NULL },
    { "*OpenRA2 " OPENRA2_VERSION, PMENU_ALIGN_RIGHT }
};

/**
 *
 */
void Cmd_Menu_f(edict_t *ent)
{
    arena_t *a = ARENA(ent);
    pmenu_t *menu = &ent->client->menu;
    uint8_t i, j;
    
    // menu is already open, close it
    if (ent->client->layout == LAYOUT_MENU) {
        PMenu_Close(ent);
        return;
    }

    PMenu_Open(ent, main_menu);

    // loop through each team building the menu
    for (i=0,j=5; i<a->team_count; i++, j++) {

        if (TEAM(ent) && (TEAM(ent) == &a->teams[i])) {
            ent->client->menu.entries[j].text = va("*Leave team %s", a->teams[i].name);
        } else {
            ent->client->menu.entries[j].text = va("*Join team %s", a->teams[i].name);
        }

        menu->entries[j].align = PMENU_ALIGN_LEFT;
        menu->entries[j].select = select_team;
    }

    // put the cursor on the first selectable item
    ent->client->menu.cur = 3;
}


/**
 *
 */
void Cmd_ArenaMenu_f(edict_t *ent)
{
    if (ent->client->layout == LAYOUT_MENU) {
        PMenu_Close(ent);
    }

    PMenu_Open(ent, arena_menu);

    int i, k;
    for (i=0, k=2; i<MAX_ARENAS; i++) {
        if (menu_lookup[i].num) {
            ent->client->menu.entries[k].text = menu_lookup[i].name;
            ent->client->menu.entries[k].align = PMENU_ALIGN_LEFT;
            ent->client->menu.entries[k].select = select_arena;
            k++;
        }
    }
    
    ent->client->menu.cur = ent->client->pers.arena->number + 1;
}

/**
 * Display the scoreboard
 */
static void Cmd_Score_f(edict_t *ent)
{
    if (ent->client->layout == LAYOUT_PLAYERS) {
        ent->client->layout = 0;
        return;
    }

    // don't toggle playerboard if playing a match
    if (ent->client->layout == LAYOUT_SCORES && ARENA(ent)->state == ARENA_STATE_PLAY) {
        ent->client->layout = 0;
        return;
    }

    if (ent->client->layout == LAYOUT_SCORES) {
        ent->client->layout = LAYOUT_PLAYERS;

        G_ArenaPlayerboardMessage(ent, true);
        return;
    }

    ent->client->layout = LAYOUT_SCORES;
    G_ArenaScoreboardMessage(ent, true);
}

/**
 * Show the playerboard
 */
static void Cmd_Playerboard_f(edict_t *ent)
{
    if (ent->client->layout == LAYOUT_PLAYERS) {
        ent->client->layout = 0;
        return;
    }

    ent->client->layout = LAYOUT_PLAYERS;
    G_ArenaPlayerboardMessage(ent, true);
}

/**
 * Look at scores from last game
 */
static void Cmd_OldScore_f(edict_t *ent)
{
    arena_t *a = ent->client->pers.arena;

    if (ent->client->layout == LAYOUT_OLDSCORES) {
        ent->client->layout = 0;
        return;
    }

    if (!a->oldscores[0]) {
        gi.cprintf(ent, PRINT_HIGH, "There is no old scoreboard yet for this arena.\n");
        return;
    }

    ent->client->layout = LAYOUT_OLDSCORES;

    gi.WriteByte(SVC_LAYOUT);
    gi.WriteString(a->oldscores);
    gi.unicast(ent, qtrue);
}

/**
 * Test function, remove later
 */
static void Cmd_Layout_f(edict_t *ent)
{
    ent->client->layout = LAYOUT_SCORES;
    
    gi.WriteByte(SVC_LAYOUT);
    gi.WriteString(gi.args());
    gi.unicast(ent, qtrue);
}

/**
 * prevent players from joining
 */
void Cmd_LockTeam_f(edict_t *ent)
{
    if (!ent->client)
        return;
    
    if (!ent->client->pers.team)
        return;
    
    arena_team_t *team = TEAM(ent);
    
    if (ent != team->captain) {
        gi.cprintf(ent, PRINT_HIGH, "Only team captains can lock/unlock their team\n");
        return;
    }
    
    if (!team->locked) {
        team->locked = true;
        gi.cprintf(ent, PRINT_HIGH, "Team '%s' now locked\n", team->name);
    } else {
        team->locked = false;
        gi.cprintf(ent, PRINT_HIGH, "Team '%s' now unlocked\n", team->name);
    }
}

/**
 * Show who is on which team
 */
static void Cmd_Teams_f(edict_t *ent)
{
    uint8_t i, j;
    arena_team_t *t;

    // actual players first
    for (i=0; i<ARENA(ent)->team_count; i++) {
        t = &ARENA(ent)->teams[i];
        gi.cprintf(ent, PRINT_HIGH, "%s <%s>:\n", t->name, t->skin);

        for (j=0; j<MAX_TEAM_PLAYERS; j++) {
            if (!t->players[j])
                continue;

            gi.cprintf(ent, PRINT_HIGH, " %s%s\n", (t->players[j] == t->captain) ? "* " : "  ", NAME(t->players[j]));
        }

        gi.cprintf(ent, PRINT_HIGH, "\n");
    }

    // now spectators
    gi.cprintf(ent, PRINT_HIGH, "Spectators\n");
    for (i=0; i<ARENA(ent)->spectator_count; i++) {
        if (ARENA(ent)->spectators[i] == NULL) {
            continue;
        }
        gi.cprintf(ent, PRINT_HIGH, "   %s\n", NAME(ARENA(ent)->spectators[i]));
    }
}

/**
 * Force all team members ready (captains only)
 */
static void Cmd_ReadyTeam_f(edict_t *ent)
{
    if (!ent->client)
        return;
    
    if (!TEAM(ent))
        return;
    
    if (ARENA(ent)->current_round > 1) {
        gi.cprintf(ent, PRINT_HIGH, "Can't change ready status mid-match\n");
        return;
    }

    arena_team_t *team = TEAM(ent);
    
    if (team->captain != ent) {
        gi.cprintf(ent, PRINT_HIGH, "Only team captains can force ready the team\n");
        return;
    }
    
    ARENA(ent)->ready_think_frame = level.framenum;
    
    G_ForceReady(team, qtrue);
    G_CheckTeamReady(team);
    G_CheckArenaReady(ARENA(ent));
}

/**
 * Remove another player from a team (captains only)
 */
static void Cmd_RemoveTeammate_f(edict_t *ent)
{
    if (!ent->client)
        return;
    
    if (!TEAM(ent))
        return;
    
    arena_team_t *team = TEAM(ent);
    
    if (team->captain != ent) {
        gi.cprintf(ent, PRINT_HIGH, "Only team captains can remove players\n");
        return;
    }
    
    if (gi.argc() < 2) {
        gi.cprintf(ent, PRINT_HIGH, "Usage: %s <playername>\n\tYou can use wildcards like * and ?\n", gi.argv(0));
        return;
    }
    
    char *namepattern = gi.argv(1);
    uint8_t matches = 0;
    edict_t *playermatch = NULL;
    uint8_t i;
    
    for (i=0; i<MAX_TEAM_PLAYERS; i++) {
        if (!team->players[i])
            continue;
        
        // don't kick yourself
        if (team->players[i] == ent)
            continue;
        
        if (match(namepattern, NAME(team->players[i]))) {
            matches++;
            playermatch = team->players[i];
        }
    }
    
    if (matches == 0) {
        gi.cprintf(ent, PRINT_HIGH, "No players matched '%s'\n", namepattern);
        return;
    }
    
    if (matches > 1) {
        gi.cprintf(ent, PRINT_HIGH, "'%s' matched %d players, try again to narrow down to a single player\n", namepattern, matches);
        return;
    }
    
    if (playermatch) {
        gi.cprintf(ent, PRINT_HIGH, "Removing %s from your team...\n", playermatch->client->pers.netname);
    
        gi.cprintf(playermatch, PRINT_HIGH, "%s removed you from team %s\n", ent->client->pers.netname, team->name);
        G_TeamPart(playermatch, false);
    }
}

/**
 * Pick a player to join your team
 */
static void Cmd_PickTeammate_f(edict_t *ent)
{
    if (!ent->client)
        return;
    
    if (!TEAM(ent))
        return;
    
    arena_t *arena = ARENA(ent);
    arena_team_t *team = TEAM(ent);
    
    if (team->captain != ent) {
        gi.cprintf(ent, PRINT_HIGH, "Only team captains can pick players\n");
        return;
    }
    
    if (gi.argc() < 2) {
        gi.cprintf(ent, PRINT_HIGH, "Usage: %s <playername>\n\tYou can use wildcards like * and ?\n", gi.argv(0));
        return;
    }
    
    if (team->player_count == MAX_TEAM_PLAYERS) {
        gi.cprintf(ent, PRINT_HIGH, "Too many players, remove someone first\n");
        return;
    }
    
    char *namepattern = gi.argv(1);
    uint8_t matches = 0;
    edict_t *playermatch = NULL;
    uint8_t i;
    
    gclient_t *c;
    
    for (i=0; i<game.maxclients; i++) {
        c = &game.clients[i];
        
        if (c->pers.connected != CONN_PREGAME && c->pers.connected != CONN_SPECTATOR)
            continue;
        
        if (c->pers.arena != arena)
            continue;
        
        // ignore caller
        if (c == ent->client)
            continue;
        
        if (match(namepattern, c->pers.netname)) {
            matches++;
            playermatch = c->edict;
        }
    }
    
    if (matches == 0) {
        gi.cprintf(ent, PRINT_HIGH, "No players matched '%s'\n", namepattern);
        return;
    }
    
    if (matches > 1) {
        gi.cprintf(ent, PRINT_HIGH, "'%s' matched %d players, try again to narrow down to a single player\n", namepattern, matches);
        return;
    }
    
    if (playermatch) {
        gi.cprintf(ent, PRINT_HIGH, "Adding %s to your team...\n", NAME(playermatch));
    
        gi.cprintf(playermatch, PRINT_HIGH, "%s added you to team %s\n", NAME(ent), team->name);
        G_TeamJoin(playermatch, team->type, true);
    }
}

/**
 * Set the skin for the team
 */
static void Cmd_TeamSkin_f(edict_t *ent)
{
    if (!ent->client)
        return;
    
    if (!TEAM(ent))
        return;
    
    arena_team_t *team = TEAM(ent);
    
    if (team->captain != ent) {
        gi.cprintf(ent, PRINT_HIGH, "Only team captains can change team skins\n");
        return;
    }
    
    if (g_skin_lock->value) {
        gi.cprintf(ent, PRINT_HIGH, "Skins are locked\n");
        return;
    }
    
    if (gi.argc() < 2) {
        gi.cprintf(ent, PRINT_HIGH, "Usage: %s <skin>\n", gi.argv(0));
        return;
    }
    
    char *skin = gi.argv(1);
    
    if (match(skin, team->skin)) {    // already that skin
        return;
    }
    
    Q_strlcpy(team->skin, skin, sizeof(team->skin));
    
    uint8_t i;
    for (i=0; i<MAX_TEAM_PLAYERS; i++) {
        if (!team->players[i])
            continue;
        
        G_SetSkin(team->players[i]);
    }
}

// placeholder for logic that hasn't been written yet
static void Cmd_NotImplYet_f(edict_t *ent)
{
    gi.cprintf(ent, PRINT_HIGH, "command not implimented yet...\n");
}

/**
 * Set custom skins per player.
 * Called by issuing "tskin" or "eskin" commands
 */
void Cmd_TeamEnemySkin_f (edict_t *ent, qboolean team)
{
    char *val;

    if (gi.argc() < 2) {
        if (team) {
            val = ent->client->pers.teamskin;
            gi.cprintf(ent, PRINT_HIGH, "Current team skin: %s\n", (!val[0])? TEAM(ent)->skin : val);
        } else {
            val = ent->client->pers.enemyskin;
            gi.cprintf(ent, PRINT_HIGH, "Current enemy skin: %s\n", (!val[0])? "<unset>" : val);
        }

        return;
    }

    if (team) {
        strncpy(ent->client->pers.teamskin, gi.argv(1),
                sizeof(ent->client->pers.teamskin) - 1);
        G_SetTSkin(ent);
    } else {
        strncpy(ent->client->pers.enemyskin, gi.argv(1),
                sizeof(ent->client->pers.enemyskin) - 1);
        G_SetESkin(ent);
    }
}
/**
 * For testing stuff
 */
static void Cmd_Test_f(edict_t *ent)
{
    gi.cprintf(ent, PRINT_HIGH, "%d\n", ent->client->pers.connected);
}

/**
 * Called for any console input not handled at the client
 */
void ClientCommand(edict_t *ent)
{
    char    *cmd;

    if (!ent->client)
        return;     // not fully in game yet

    if (ent->client->pers.connected <= CONN_CONNECTED) {
        return;
    }

    cmd = gi.argv(0);

    if (ent->client->pers.admin) {
        if (Q_stricmp(cmd, "mute") == 0) {
            Cmd_Mute_f(ent, qtrue);
            return;
        }
        if (Q_stricmp(cmd, "unmute") == 0) {
            Cmd_Mute_f(ent, qfalse);
            return;
        }
        if (Q_stricmp(cmd, "muteall") == 0) {
            Cmd_MuteAll_f(ent, qtrue);
            return;
        }
        if (Q_stricmp(cmd, "unmuteall") == 0) {
            Cmd_MuteAll_f(ent, qfalse);
            return;
        }
        if (Q_stricmp(cmd, "ban") == 0) {
            G_AddIP_f(ent);
            return;
        }
        if (Q_stricmp(cmd, "unban") == 0) {
            G_RemoveIP_f(ent);
            return;
        }
        if (Q_stricmp(cmd, "bans") == 0) {
            G_ListIP_f(ent);
            return;
        }
        if (Q_stricmp(cmd, "kick") == 0 || Q_stricmp(cmd, "boot") == 0) {
            Cmd_Kick_f(ent, qfalse);
            return;
        }
        if (Q_stricmp(cmd, "kickban") == 0) {
            Cmd_Kick_f(ent, qtrue);
            return;
        }
        if (Q_stricmp(cmd, "acommands") == 0) {
            Cmd_AdminCommands_f(ent);
            return;
        }
    }

    if (Q_stricmp(cmd, "say_all") == 0) {
        Cmd_Say_f(ent, CHAT_ALL);
        return;
    }
    if (Q_stricmp(cmd, "say") == 0 || Q_stricmp(cmd, "say_arena") == 0) {
        Cmd_Say_f(ent, CHAT_ARENA);
        return;
    }
    if (Q_stricmp(cmd, "say_team") == 0) {
        Cmd_Say_f(ent, CHAT_TEAM);
        return;
    }
    if (Q_stricmp(cmd, "players") == 0 || Q_stricmp(cmd, "playerlist") == 0) {
        Cmd_Players_f(ent);
        return;
    }
    if (Q_stricmp(cmd, "highscore") == 0 || Q_stricmp(cmd, "highscores") == 0) {
        Cmd_HighScores_f(ent);
        return;
    }
    if (Q_stricmp(cmd, "stats") == 0 || Q_stricmp(cmd, "accuracy") == 0) {
        Cmd_Stats_f(ent, qtrue);
        return;
    }
    if (Q_stricmp(cmd, "settings") == 0 || Q_stricmp(cmd, "matchinfo") == 0) {
        Cmd_Settings_f(ent);
        return;
    }
    if (Q_stricmp(cmd, "admin") == 0 || Q_stricmp(cmd, "referee") == 0) {
        Cmd_Admin_f(ent);
        return;
    }
    if (Q_stricmp(cmd, "commands") == 0) {
        Cmd_Commands_f(ent);
        return;
    }
    if (Q_stricmp(cmd, "id") == 0) {
        Cmd_Id_f(ent);
        return;
    }

    if (ARENA(ent)->intermission_framenum)
        return;

    if (Q_stricmp(cmd, "score") == 0 || Q_stricmp(cmd, "help") == 0)
        Cmd_Score_f(ent);
    else if (Q_stricmp(cmd, "oldscore") == 0 || Q_stricmp(cmd, "oldscores") == 0 ||
             Q_stricmp(cmd, "lastscore") == 0 || Q_stricmp(cmd, "lastscores") == 0)
        Cmd_OldScore_f(ent);
    else if (Q_stricmp(cmd, "playerboard") == 0)
            Cmd_Playerboard_f(ent);
    else if (Q_stricmp(cmd, "use") == 0)
        Cmd_Use_f(ent);
    else if (Q_stricmp(cmd, "drop") == 0)
        Cmd_Drop_f(ent);
    else if (Q_stricmp(cmd, "give") == 0)
        Cmd_Give_f(ent);
    else if (Q_stricmp(cmd, "god") == 0)
        Cmd_God_f(ent);
    else if (Q_stricmp(cmd, "notarget") == 0)
        Cmd_Notarget_f(ent);
    else if (Q_stricmp(cmd, "noclip") == 0)
        Cmd_Noclip_f(ent);
    else if (Q_stricmp(cmd, "inven") == 0)
        Cmd_Inven_f(ent);
    else if (Q_stricmp(cmd, "invnext") == 0)
        SelectNextItem(ent, -1);
    else if (Q_stricmp(cmd, "invprev") == 0)
        SelectPrevItem(ent, -1);
    else if (Q_stricmp(cmd, "invnextw") == 0)
        SelectNextItem(ent, IT_WEAPON);
    else if (Q_stricmp(cmd, "invprevw") == 0)
        SelectPrevItem(ent, IT_WEAPON);
    else if (Q_stricmp(cmd, "invnextp") == 0)
        SelectNextItem(ent, IT_POWERUP);
    else if (Q_stricmp(cmd, "invprevp") == 0)
        SelectPrevItem(ent, IT_POWERUP);
    else if (Q_stricmp(cmd, "invuse") == 0)
        Cmd_InvUse_f(ent);
    else if (Q_stricmp(cmd, "invdrop") == 0)
        Cmd_InvDrop_f(ent);
    else if (Q_stricmp(cmd, "weapprev") == 0)
        Cmd_WeapPrev_f(ent);
    else if (Q_stricmp(cmd, "weapnext") == 0)
        Cmd_WeapNext_f(ent);
    else if (Q_stricmp(cmd, "weaplast") == 0)
        Cmd_WeapLast_f(ent);
    else if (Q_stricmp(cmd, "kill") == 0)
        Cmd_Kill_f(ent);
    else if (Q_stricmp(cmd, "putaway") == 0)
        Cmd_PutAway_f(ent);
    else if (Q_stricmp(cmd, "wave") == 0)
        Cmd_Wave_f(ent);
    else if (Q_stricmp(cmd, "observe") == 0 || Q_stricmp(cmd, "spectate") == 0 ||
             Q_stricmp(cmd, "spec") == 0 || Q_stricmp(cmd, "obs") == 0 ||
             Q_stricmp(cmd, "observer") == 0 || Q_stricmp(cmd, "spectator") == 0)
        Cmd_Observe_f(ent);
    else if (Q_stricmp(cmd, "chase") == 0)
        Cmd_Chase_f(ent);
    else if (Q_stricmp(cmd, "join") == 0)
        Cmd_Team_f(ent);
    else if (Q_stricmp(cmd, "vote") == 0 || Q_stricmp(cmd, "callvote") == 0)
        Cmd_Vote_f(ent);
    else if (Q_stricmp(cmd, "yes") == 0 && (level.vote.proposal || ARENA(ent)->vote.proposal))
        Cmd_CastVote_f(ent, qtrue);
    else if (Q_stricmp(cmd, "no") == 0 && (level.vote.proposal || ARENA(ent)->vote.proposal))
        Cmd_CastVote_f(ent, qfalse);
    else if (Q_stricmp(cmd, "menu") == 0)
        Cmd_Menu_f(ent);
    else if (Q_stricmp(cmd, "arena") == 0)
        Cmd_Arena_f(ent);
    else if (Q_stricmp(cmd, "team") == 0)
        Cmd_Team_f(ent);
    else if (Q_stricmp(cmd, "ready") == 0)
        Cmd_Ready_f(ent);
    else if (Q_stricmp(cmd, "lock") == 0 || Q_stricmp(cmd, "lockteam") == 0)
        Cmd_LockTeam_f(ent);
    else if (Q_stricmp(cmd, "time") == 0 || Q_stricmp(cmd, "timeout") == 0 || Q_stricmp(cmd, "timein") == 0)
        Cmd_Timeout_f(ent);
    else if (Q_stricmp(cmd, "readyteam") == 0 || Q_stricmp(cmd, "readyall") == 0 || Q_stricmp(cmd, "forceready") == 0) // captain cmd
        Cmd_ReadyTeam_f(ent);
    else if (Q_stricmp(cmd, "kickplayer") == 0 || Q_stricmp(cmd, "remove") == 0)    // captain cmd, remove player from team
        Cmd_RemoveTeammate_f(ent);
    else if (Q_stricmp(cmd, "pickplayer") == 0 || Q_stricmp(cmd, "pick") == 0)    // captain cmd, pick player for a team
        Cmd_PickTeammate_f(ent);
    else if (Q_stricmp(cmd, "teamskin") == 0)    // captain cmd
        Cmd_TeamSkin_f(ent);
    else if (Q_stricmp(cmd, "tskin") == 0)
        Cmd_TeamEnemySkin_f(ent, true);
    else if (Q_stricmp(cmd, "eskin") == 0)
        Cmd_TeamEnemySkin_f(ent, false);
    else if (Q_stricmp(cmd, "layout") == 0) // test
        Cmd_Layout_f(ent);
    else if (Q_stricmp(cmd, "teams") == 0) // test
        Cmd_Teams_f(ent);
    else if (Q_stricmp(cmd, "sound") == 0) // test
        Cmd_Sound_f(ent);
    else if (Q_stricmp(cmd, "test") == 0) {
        Cmd_Test_f(ent);
        Cmd_NotImplYet_f(ent);
    }
    else    // anything that doesn't match a command will be a chat
        Cmd_Say_f(ent, CHAT_MISC);
}

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

/*
======================================================================

RANKING / SCOREBOARD

======================================================================
*/

int G_PlayerCmp(const void *p1, const void *p2) {
    gclient_t *a = *(gclient_t * const *)p1;
    gclient_t *b = *(gclient_t * const *)p2;

    if (a->resp.damage_given > b->resp.damage_given) {
        return -1;
    }
    if (a->resp.damage_given < b->resp.damage_given) {
        return 1;
    }
    if (a->resp.damage_recvd < b->resp.damage_recvd) {
        return -1;
    }
    if (a->resp.damage_recvd > b->resp.damage_recvd) {
        return 1;
    }
    return 0;
}

int G_CalcRanks(gclient_t **ranks)
{
    int i, total;

    // sort the clients by score, then by eff
    total = 0;
    for (i = 0; i < game.maxclients; i++) {
        if (game.clients[i].pers.connected == CONN_SPAWNED) {
            if (ranks) {
                ranks[total] = &game.clients[i];
            }
            total++;
        }
    }

    if (ranks) {
        qsort(ranks, total, sizeof(gclient_t *), G_PlayerCmp);
    }

    return total;
}


/*
==================
BuildScoreboard

Used to update per-client scoreboard and build
global oldscores (client is NULL in the latter case).
==================
*/
static size_t BuildScoreboard(char *buffer, gclient_t *client)
{
    char    entry[MAX_STRING_CHARS];
    char    status[MAX_QPATH];
    char    timebuf[16];
    size_t  total, len;
    int     i, j, numranks;
    int     y, sec, eff;
    gclient_t   *ranks[MAX_CLIENTS];
    gclient_t   *c;
    time_t      t;
    struct tm   *tm;

    if (!client) {
        Q_snprintf(entry, sizeof(entry),
                   "yv 10 cstring2 \"Old scoreboard from %s\"", level.mapname);
    } else {
        entry[0] = 0;
    }

    t = time(NULL);
    tm = localtime(&t);
    len = strftime(status, sizeof(status), "[%Y-%m-%d %H:%M]", tm);
    if (len < 1)
        strcpy(status, "???");

    total = Q_scnprintf(buffer, MAX_STRING_CHARS,
                        "xv 0 %s"
                        "yv 18 "
                        "cstring \"%s\""
                        "xv -16 "
                        "yv 26 "
                        "string \"Player          Frg Dth Eff%% FPH Time Ping\""
                        "xv -40 ", entry, status);

    numranks = G_CalcRanks(ranks);

    // add the clients sorted by rank
    y = 34;
    for (i = 0; i < numranks; i++) {
        c = ranks[i];

        sec = (level.framenum - c->resp.enter_framenum) / HZ;
        if (!sec) {
            sec = 1;
        }

        if (c->resp.score > 0) {
            j = c->resp.score + c->resp.deaths;
            eff = j ? c->resp.score * 100 / j : 100;
        } else {
            eff = 0;
        }

        if (level.framenum < 10 * 60 * HZ) {
            sprintf(timebuf, "%d:%02d", sec / 60, sec % 60);
        } else {
            sprintf(timebuf, "%d", sec / 60);
        }

        len = Q_snprintf(entry, sizeof(entry),
                         "yv %d string%s \"%2d %-15s %3d %3d %3d %4d %4s %4d\"",
                         y, c == client ? "" : "2", i + 1,
                         c->pers.netname, c->resp.score, c->resp.deaths, eff,
                         c->resp.score * 3600 / sec, timebuf, c->ping);
        if (len >= sizeof(entry)) {
            continue;
        }
        if (total + len >= MAX_STRING_CHARS)
            break;
        memcpy(buffer + total, entry, len);
        total += len;
        y += 8;
    }

    // add spectators in fixed order
    for (i = 0, j = 0; i < game.maxclients; i++) {
        c = &game.clients[i];
        if (c->pers.connected != CONN_PREGAME && c->pers.connected != CONN_SPECTATOR) {
            continue;
        }
        if (c->pers.mvdspec) {
            continue;
        }

        sec = (level.framenum - c->resp.enter_framenum) / HZ;
        if (!sec) {
            sec = 1;
        }

        if (c->chase_target) {
            Q_snprintf(status, sizeof(status), "(-> %.13s)",
                       c->chase_target->client->pers.netname);
        } else {
            strcpy(status, "(observing)");
        }

        len = Q_snprintf(entry, sizeof(entry),
                         "yv %d string%s \"   %-15s %-18s%3d %4d\"",
                         y, c == client ? "" : "2",
                         c->pers.netname, status, sec / 60, c->ping);
        if (len >= sizeof(entry)) {
            continue;
        }
        if (total + len >= MAX_STRING_CHARS)
            break;
        memcpy(buffer + total, entry, len);
        total += len;
        y += 8;
        j++;
    }

    // add server info
    if (sv_hostname && sv_hostname->string[0]) {
        len = Q_scnprintf(entry, sizeof(entry), "xl 8 yb -37 string2 \"%s\"",
                          sv_hostname->string);
        if (total + len < MAX_STRING_CHARS) {
            memcpy(buffer + total, entry, len);
            total += len;
        }
    }

    buffer[total] = 0;

    return total;
}

/*
==================
HighScoresMessage

Sent to all clients during intermission.
==================
*/
void HighScoresMessage(void)
{
    char    entry[MAX_STRING_CHARS];
    char    string[MAX_STRING_CHARS];
    char    date[MAX_QPATH];
    struct tm   *tm;
    score_t *s;
    size_t  total, len;
    int     i;
    int     y;

    total = Q_snprintf(string, sizeof(string),
                       "xv 0 "
                       "yv 0 "
                       "cstring \"High Scores for %s\""
                       "yv 16 "
                       "cstring2 \"  # Name             FPH Date      \"",
                       level.mapname);

    y = 24;
    for (i = 0; i < level.numscores; i++) {
        s = &level.scores[i];

        tm = localtime(&s->time);
        len = strftime(date, sizeof(date), "%Y-%m-%d", tm);
        if (len < 1) {
            strcpy(date, "???");
        }
        len = Q_snprintf(entry, sizeof(entry),
                         "yv %d cstring \"%c%2d %-15.15s %4d %-10s\"",
                         y, s->time == level.record ? '*' : ' ',
                         i + 1, s->name, s->score, date);
        if (len >= sizeof(entry)) {
            continue;
        }
        if (total + len >= MAX_STRING_CHARS)
            break;
        memcpy(string + total, entry, len);
        total += len;
        y += 8;
    }
    string[total] = 0;

    gi.WriteByte(SVC_LAYOUT);
    gi.WriteString(string);
    gi.multicast(NULL, MULTICAST_ALL_R);
}

/*
==================

Can go either as reliable message (manual updates, intermission)
and unreliable (automatic). Note that it isn't that hard to overflow
the 1024 chars layout size limit!
==================
*/
void ScoreboardMessage(edict_t *ent, qboolean reliable)
{
    char buffer[MAX_STRING_CHARS];

    BuildScoreboard(buffer, ent->client);

    gi.WriteByte(SVC_LAYOUT);
    gi.WriteString(buffer);
    gi.unicast(ent, reliable);
}


/*
======================================================================

INTERMISSION

======================================================================
*/

void MoveClientToIntermission(edict_t *ent)
{
	arena_t *a = ARENA(ent);
	
    PMenu_Close(ent);
	
    ent->client->layout = LAYOUT_SCORES;
    VectorCopy(a->intermission_origin, ent->s.origin);
    ent->client->ps.pmove.origin[0] = a->intermission_origin[0] * 8;
    ent->client->ps.pmove.origin[1] = a->intermission_origin[1] * 8;
    ent->client->ps.pmove.origin[2] = a->intermission_origin[2] * 8;
    VectorCopy(a->intermission_angle, ent->client->ps.viewangles);
    ent->client->ps.pmove.pm_type = PM_FREEZE;
    ent->client->ps.pmove.pm_flags ^= PMF_TELEPORT_BIT;
    ent->client->ps.gunindex = 0;
    ent->client->ps.blend[3] = 0;
    ent->client->ps.rdflags &= ~RDF_UNDERWATER;
    ent->client->ps.stats[STAT_FLASHES] = 0;

    // clean up powerup info
    ent->client->quad_framenum = 0;
    ent->client->invincible_framenum = 0;
    ent->client->breather_framenum = 0;
    ent->client->enviro_framenum = 0;
    ent->client->grenade_state = GRENADE_NONE;
    ent->client->grenade_framenum = 0;

    ent->watertype = 0;
    ent->waterlevel = 0;
    ent->viewheight = 0;
    ent->s.modelindex = 0;
    ent->s.modelindex2 = 0;
    ent->s.modelindex3 = 0;
    ent->s.modelindex4 = 0;
    ent->s.effects = 0;
    ent->s.renderfx = 0;
    ent->s.sound = 0;
    ent->s.event = 0;
    ent->s.solid = 0;
    ent->solid = SOLID_NOT;
    ent->svflags = SVF_NOCLIENT;
    gi.unlinkentity(ent);

    if (PLAYER_SPAWNED(ent) || ent->client->chase_target) {
        Cmd_Stats_f(ent, qfalse);
    }

    // add the layout
    G_ArenaScoreboardMessage(ent, qtrue);

    if (ent->client->pers.uf & UF_AUTOSCREENSHOT) {
        //G_StuffText(ent, "wait; screenshot\n");
    }
}

void BeginIntermission(arena_t *a)
{
    int        i;
    edict_t    *ent, *client;

	if (!a)
		return;
	
    if (a->intermission_framenum)
        return;

    a->state = ARENA_STATE_INTERMISSION;
    a->intermission_framenum = level.framenum;

    G_FinishVote(); // ? maybe not

    // respawn any dead clients
    for (i = 0; i < game.maxclients; i++) {
        client = g_edicts + 1 + i;
		
        if (!client->inuse) {
            continue;
		}
		
        if (client->health <= 0 && ARENA(client) == a) {
            respawn(client);
		}
    }

	ent = SelectIntermissionPoint(a);
	
    if (ent) {
        VectorCopy(ent->s.origin, a->intermission_origin);
        VectorCopy(ent->s.angles, a->intermission_angle);
    }

    // move all clients in this arena to the intermission point
    for (i = 0; i < game.maxclients; i++) {
        client = g_edicts + 1 + i;
		
        if (!client->inuse) {
            continue;
		}
		
		if (ARENA(client) == a) {
			MoveClientToIntermission(client);
		}
    }
}

//=======================================================================

void G_PrivateString(edict_t *ent, int index, const char *string)
{
    gclient_t *client;
    int i;

    if (index < 0 || index >= PCS_TOTAL) {
        gi.error("%s: index %d out of range", __func__, index);
    }

    if (!strcmp(ent->client->level.strings[index], string)) {
        return; // not changed
    }

    // save new string
    Q_strlcpy(ent->client->level.strings[index], string, MAX_NETNAME);

    gi.WriteByte(SVC_CONFIGSTRING);
    gi.WriteShort(CS_PRIVATE + index);
    gi.WriteString(string);
    gi.unicast(ent, qtrue);

    // send it to chasecam clients too
    if (ent->client->chase_target) {
        return;
    }
    for (i = 0, client = game.clients; i < game.maxclients; i++, client++) {
        if (client->pers.connected != CONN_SPECTATOR) {
            continue;
        }
        if (client->chase_target == ent) {
            G_PrivateString(client->edict, index, string);
        }
    }
}

/*
=============
visible

returns 1 if the entity is visible to self, even if not infront ()
=============
*/
static qboolean visible(edict_t *self, edict_t *other, int mask)
{
    vec3_t  spot1;
    vec3_t  spot2;
    trace_t trace;
    int     i;

    VectorCopy(self->s.origin, spot1);
    spot1[2] += self->viewheight;

    VectorCopy(other->s.origin, spot2);
    spot2[2] += other->viewheight;

    for (i = 0; i < 10; i++) {
        trace = gi.trace(spot1, vec3_origin, vec3_origin, spot2, self, mask);

        if (trace.fraction == 1.0)
            return qtrue;

        // entire move is inside water volume
        if (trace.allsolid && (trace.contents & MASK_WATER)) {
            mask &= ~MASK_WATER;
            continue;
        }

        // hit transparent water
        if (trace.ent == world && trace.surface &&
            (trace.surface->flags & (SURF_TRANS33 | SURF_TRANS66))) {
            mask &= ~MASK_WATER;
            VectorCopy(trace.endpos, spot1);
            continue;
        }

        break;
    }
    return qfalse;
}

/*
==============
TDM_GetPlayerIdView

Find the best player for the id view and return configstring index.
Code below comes from OpenTDM.
==============
*/
static edict_t *find_by_tracing(edict_t *ent)
{
    edict_t     *ignore;
    vec3_t      forward;
    trace_t     tr;
    vec3_t      start;
    vec3_t      mins = { -4, -4, -4 };
    vec3_t      maxs = { 4, 4, 4 };
    int         i;
    int         tracemask;

    VectorCopy(ent->s.origin, start);
    start[2] += ent->viewheight;

    AngleVectors(ent->client->v_angle, forward, NULL, NULL);

    VectorScale(forward, 4096, forward);
    VectorAdd(ent->s.origin, forward, forward);

    ignore = ent;

    tracemask = CONTENTS_SOLID | CONTENTS_MONSTER | MASK_WATER;

    // find best player through tracing
    for (i = 0; i < 10; i++) {
        tr = gi.trace(start, mins, maxs, forward, ignore, tracemask);

        // entire move is inside water volume
        if (tr.allsolid && (tr.contents & MASK_WATER)) {
            tracemask &= ~MASK_WATER;
            continue;
        }

        // hit transparent water
        if (tr.ent == world && tr.surface &&
            (tr.surface->flags & (SURF_TRANS33 | SURF_TRANS66))) {
            tracemask &= ~MASK_WATER;
            VectorCopy(tr.endpos, start);
            continue;
        }

        if (tr.ent == world || tr.fraction == 1.0f)
            break;

        // we hit something that's a player and it's alive!
        // note, we trace twice so we hit water planes
        if (tr.ent && tr.ent->client && tr.ent->health > 0 &&
            visible(tr.ent, ent, CONTENTS_SOLID | MASK_WATER)) {
            return tr.ent;
        }

        VectorCopy(tr.endpos, start);
        ignore = tr.ent;
    }

    return NULL;
}

static edict_t *find_by_angles(edict_t *ent)
{
    vec3_t      forward;
    edict_t     *who, *best;
    vec3_t      dir;
    float       distance, bdistance = 0.0f;
    float       bd = 0.0f, d;

    AngleVectors(ent->client->v_angle, forward, NULL, NULL);
    best = NULL;

    // if trace was unsuccessful, try guessing based on angles
    for (who = g_edicts + 1; who <= g_edicts + game.maxclients; who++) {
        if (!who->inuse)
            continue;
        if (!PLAYER_SPAWNED(who))
            continue;
        if (who->health <= 0)
            continue;

        if (who == ent)
            continue;

        VectorSubtract(who->s.origin, ent->s.origin, dir);
        distance = VectorLength(dir);

        VectorNormalize(dir);
        d = DotProduct(forward, dir);

        // note, we trace twice so we hit water planes
        if (d > bd &&
            visible(ent, who, CONTENTS_SOLID | MASK_WATER) &&
            visible(who, ent, CONTENTS_SOLID | MASK_WATER)) {
            bdistance = distance;
            bd = d;
            best = who;
        }
    }

    if (!best) {
        return NULL;
    }

    // allow variable slop based on proximity
    if ((bdistance < 150 && bd > 0.50f) ||
        (bdistance < 250 && bd > 0.90f) ||
        (bdistance < 600 && bd > 0.96f) ||
        bd > 0.98f) {
        return best;
    }

    return NULL;
}

/**
 * Figure out which player ent is looking at.
 * Returns that player's configstring index and sets
 * the teammate pointer appropriately.
 */
int G_GetPlayerIdView(edict_t *ent, qboolean *teammate)
{
    edict_t *target;

    target = find_by_tracing(ent);
    if (!target) {
        target = find_by_angles(ent);
        if (!target) {
            return 0;
        }
    }

    if (G_Teammates(ent, target)) {
        *teammate = qtrue;
    }

    return CS_PLAYERNAMES + (target - g_edicts) - 1;
}



/*
===============
G_SetStats
===============
*/
void G_SetStats(edict_t *ent)
{
    const gitem_t   *item;
    int             index, cells;
    int             power_armor_type;
    qboolean        teammate = qfalse;
    int             viewid = 0;

    //
    // health
    //
    ent->client->ps.stats[STAT_HEALTH_ICON] = level.images.health;
    ent->client->ps.stats[STAT_HEALTH] = ent->health;

    //
    // ammo
    //
    if (!ent->client->ammo_index /* || !ent->client->pers.inventory[ent->client->ammo_index] */) {
        ent->client->ps.stats[STAT_AMMO_ICON] = 0;
        ent->client->ps.stats[STAT_AMMO] = 0;
    } else {
        item = INDEX_ITEM(ent->client->ammo_index);
        ent->client->ps.stats[STAT_AMMO_ICON] = gi.imageindex(item->icon);
        ent->client->ps.stats[STAT_AMMO] = ent->client->inventory[ent->client->ammo_index];
        ent->client->ps.stats[STAT_WEAPON_ICON] = gi.imageindex(ent->client->weapon->icon);
    }

    //
    // armor
    //
    cells = 0;
    power_armor_type = PowerArmorIndex(ent);
    if (power_armor_type) {
        cells = ent->client->inventory[ITEM_CELLS];
        if (cells == 0) {
            // ran out of cells for power armor
            ent->flags &= ~FL_POWER_ARMOR;
            gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/power2.wav"), 1, ATTN_NORM, 0);
            power_armor_type = 0;
        }
    }

    index = ArmorIndex(ent);
    if (power_armor_type && (!index || ((level.framenum / FRAMEDIV) & 8))) {
        // flash between power armor and other armor icon
        ent->client->ps.stats[STAT_ARMOR_ICON] = level.images.powershield;
        ent->client->ps.stats[STAT_ARMOR] = cells;
    } else if (index) {
        item = INDEX_ITEM(index);
        ent->client->ps.stats[STAT_ARMOR_ICON] = gi.imageindex(item->icon);
        ent->client->ps.stats[STAT_ARMOR] = ent->client->inventory[index];
    } else {
        ent->client->ps.stats[STAT_ARMOR_ICON] = 0;
        ent->client->ps.stats[STAT_ARMOR] = 0;
    }

    //
    // timer 1 (quad, enviro, breather)
    //
    if (ent->client->quad_framenum > level.framenum) {
        ent->client->ps.stats[STAT_TIMER_ICON] = level.images.quad;
        ent->client->ps.stats[STAT_TIMER] = (ent->client->quad_framenum - level.framenum) / HZ;
    } else if (ent->client->enviro_framenum > level.framenum) {
        ent->client->ps.stats[STAT_TIMER_ICON] = level.images.envirosuit;
        ent->client->ps.stats[STAT_TIMER] = (ent->client->enviro_framenum - level.framenum) / HZ;
    } else if (ent->client->breather_framenum > level.framenum) {
        ent->client->ps.stats[STAT_TIMER_ICON] = level.images.rebreather;
        ent->client->ps.stats[STAT_TIMER] = (ent->client->breather_framenum - level.framenum) / HZ;
    } else {
        ent->client->ps.stats[STAT_TIMER_ICON] = 0;
        ent->client->ps.stats[STAT_TIMER] = 0;
    }

    //
    // timer 2 (pent)
    //
    /*
    ent->client->ps.stats[STAT_TIMER2_ICON] = 0;
    ent->client->ps.stats[STAT_TIMER2] = 0;
    if (ent->client->invincible_framenum > level.framenum) {
        if (ent->client->ps.stats[STAT_TIMER_ICON]) {
            ent->client->ps.stats[STAT_TIMER2_ICON] = level.images.invulnerability;
            ent->client->ps.stats[STAT_TIMER2] = (ent->client->invincible_framenum - level.framenum) / HZ;
        } else {
            ent->client->ps.stats[STAT_TIMER_ICON] = level.images.invulnerability;
            ent->client->ps.stats[STAT_TIMER] = (ent->client->invincible_framenum - level.framenum) / HZ;
        }
    }
    */

    //
    // selected item
    //
    if (ent->client->selected_item == -1) {
        ent->client->ps.stats[STAT_SELECTED_ICON] = 0;
    } else {
        item = INDEX_ITEM(ent->client->selected_item);
        ent->client->ps.stats[STAT_SELECTED_ICON] = gi.imageindex(item->icon);
    }

    ent->client->ps.stats[STAT_SELECTED_ITEM] = ent->client->selected_item;

    //
    // layouts
    //
    ent->client->ps.stats[STAT_LAYOUTS] = 0;

    if (ent->health <= 0 || level.intermission_framenum || ent->client->layout)
        ent->client->ps.stats[STAT_LAYOUTS] |= 1;

    //
    // frags
    //
    ent->client->ps.stats[STAT_FRAGS] = ent->client->resp.damage_given;

    //
    // help icon / current weapon if not shown
    //
    //if ((ent->client->pers.hand == CENTER_HANDED || ent->client->ps.fov > 91) && ent->client->weapon)
    //    ent->client->ps.stats[STAT_HELPICON] = gi.imageindex(ent->client->weapon->icon);
    //else
    //    ent->client->ps.stats[STAT_HELPICON] = 0;

    ent->client->ps.stats[STAT_SPECTATOR] = 0;
    ent->client->ps.stats[STAT_CHASE] = 0;

    if (level.intermission_framenum || ARENA(ent)->round_intermission_start) {
        ent->client->ps.stats[STAT_VIEWID] = 0;
    } else {

        if (ent->client->pers.connected == CONN_SPAWNED) {
			
			// countdown
			if (ent->client->pers.arena->state == ARENA_STATE_COUNTDOWN) {
				ent->client->ps.stats[STAT_COUNTDOWN] = ARENA(ent)->countdown;
			} else {
				ent->client->ps.stats[STAT_COUNTDOWN] = 0;
			}
			
        } else {

            if (ent->client->pers.connected == CONN_SPECTATOR) {
                ent->client->ps.stats[STAT_SPECTATOR] = CS_SPECMODE;
            } else {
                ent->client->ps.stats[STAT_SPECTATOR] = CS_PREGAME;
            }
        }
        if (ent->client->pers.noviewid) {
            ent->client->ps.stats[STAT_VIEWID] = 0;
            ent->client->ps.stats[STAT_VIEWID_TEAM] = 0;
        } else {
            viewid = G_GetPlayerIdView(ent, &teammate);
            if (teammate) {
                ent->client->ps.stats[STAT_VIEWID] = viewid;
                ent->client->ps.stats[STAT_VIEWID_TEAM] = 1;
            } else {
                ent->client->ps.stats[STAT_VIEWID] = 0;
                ent->client->ps.stats[STAT_VIEWID_TEAM] = 0;
            }
        }
    }

    if (level.vote.proposal && VF(SHOW)) {
        ent->client->ps.stats[STAT_VOTE_PROPOSAL] = CS_VOTE_PROPOSAL;
        ent->client->ps.stats[STAT_VOTE_COUNT] = CS_VOTE_COUNT;
    } else if (ARENA(ent)->vote.proposal) {
    	ent->client->ps.stats[STAT_VOTE_PROPOSAL] = CS_VOTE_PROPOSAL;
    	ent->client->ps.stats[STAT_VOTE_COUNT] = CS_VOTE_COUNT;
    } else {
        ent->client->ps.stats[STAT_VOTE_PROPOSAL] = 0;
        ent->client->ps.stats[STAT_VOTE_COUNT] = 0;
    }
	
	if (ARENA(ent)->state == ARENA_STATE_WARMUP) {
		if (TEAM(ent)) {
			if (!ent->client->pers.ready) {
				ent->client->ps.stats[STAT_READY] = CS_READY;
			} else {
				ent->client->ps.stats[STAT_READY] = CS_READY_WAIT;
			}
		} else {
			ent->client->ps.stats[STAT_READY] = 0;
		}
	} else {
		ent->client->ps.stats[STAT_READY] = 0;
	}

	ent->client->ps.stats[STAT_MATCH_STATUS] = CS_MATCH_STATUS;

	if (TEAM(ent)) {
		ent->client->ps.stats[STAT_AMMO_BULLETS] = ent->client->inventory[ITEM_BULLETS];
		ent->client->ps.stats[STAT_AMMO_SHELLS] = ent->client->inventory[ITEM_SHELLS];
		ent->client->ps.stats[STAT_AMMO_GRENADES] = ent->client->inventory[ITEM_GRENADES];
		ent->client->ps.stats[STAT_AMMO_CELLS] = ent->client->inventory[ITEM_CELLS];
		ent->client->ps.stats[STAT_AMMO_ROCKETS] = ent->client->inventory[ITEM_ROCKETS];
		ent->client->ps.stats[STAT_AMMO_SLUGS] = ent->client->inventory[ITEM_SLUGS];
	}

	ent->client->ps.stats[STAT_ROUND] = CS_ROUND;
}


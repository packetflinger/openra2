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

/**
 * Returns true if the inflicter can directly damage the target.
 * Used for explosions and melee attacks.
 */
qboolean CanDamage(edict_t *targ, edict_t *inflictor) {
    vec3_t  dest;
    trace_t trace;
    int i;

    // bmodels need special checking because their origin is 0,0,0
    if (targ->movetype == MOVETYPE_PUSH) {
        VectorAdd(targ->absmin, targ->absmax, dest);
        VectorScale(dest, 0.5, dest);
        trace = gi.trace(inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
        if (trace.fraction == 1.0) {
            return qtrue;
        }
        if (trace.ent == targ) {
            return qtrue;
        }
        return qfalse;
    }

    trace = gi.trace(inflictor->s.origin, vec3_origin, vec3_origin, targ->s.origin, inflictor, MASK_SOLID);
    if (trace.fraction == 1.0) {
        return qtrue;
    }

    if ((int)g_bugs->value < 1) {
        vec_t *bounds[] = { targ->absmin, targ->absmax };

        for (i = 0; i < 8; i++) {
            dest[0] = bounds[(i >> 0) & 1][0];
            dest[1] = bounds[(i >> 1) & 1][1];
            dest[2] = bounds[(i >> 2) & 1][2];

            trace = gi.trace(inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
            if (trace.fraction == 1.0) {
                return qtrue;
            }
        }
    } else {
        dest[2] = targ->s.origin[2];
        for (i = 0; i < 4; i++) {
            if (i & 1) {
                dest[0] = targ->s.origin[0] - 15.0f;
            } else {
                dest[0] = targ->s.origin[0] + 15.0f;
            }
            if (i & 2) {
                dest[1] = targ->s.origin[1] - 15.0f;
            } else {
                dest[1] = targ->s.origin[1] + 15.0f;
            }

            trace = gi.trace(inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
            if (trace.fraction == 1.0) {
                return qtrue;
            }
        }
    }
    return qfalse;
}


/**
 * Called from T_Damage if targ's health falls below to 0 or below
 *
 * targ = who died
 * inflictor = edict that did the killing (rocket, grenade)
 * attacker = edict who owns inflictor
 * damage = the amount of damage applied
 * point = where it happened
 */
static void Killed(edict_t *targ, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point) {
    if (targ->health < -999) {
        targ->health = -999;
    }
    targ->enemy = attacker;
    targ->killer = attacker;
    targ->die(targ, inflictor, attacker, damage, point);
}

/**
 * Show damage in the world
 */
static void SpawnDamage(int type, vec3_t origin, vec3_t normal) {
    gi.WriteByte(SVC_TEMP_ENTITY);
    gi.WriteByte(type);
    gi.WritePosition(origin);
    gi.WriteDir(normal);
    gi.multicast(origin, MULTICAST_PVS);
}

/**
 * See how much damage power armor absorbs
 */
static int CheckPowerArmor(edict_t *ent, vec3_t point, vec3_t normal, int damage, int dflags) {
    gclient_t   *client;
    int         save;
    int         power_armor_index;
    int         damagePerCell;
    int         pa_te_type;
    int         power;
    int         power_used;

    if (!damage) {
        return 0;
    }

    client = ent->client;
    if (!client) {
        return 0;
    }

    if (dflags & DAMAGE_NO_ARMOR) {
        return 0;
    }

    power_armor_index = PowerArmorIndex(ent);
    if (!power_armor_index) {
        return 0;
    }

    power = client->inventory[ITEM_CELLS];
    if (!power) {
        return 0;
    }

    if (power_armor_index == ITEM_POWER_SCREEN) {
        vec3_t      vec;
        float       dot;
        vec3_t      forward;

        // only works if damage point is in front
        AngleVectors(ent->s.angles, forward, NULL, NULL);
        VectorSubtract(point, ent->s.origin, vec);
        VectorNormalize(vec);
        dot = DotProduct(vec, forward);
        if (dot <= 0.3) {
            return 0;
        }

        damagePerCell = 1;
        pa_te_type = TE_SCREEN_SPARKS;
        damage = damage / 3;
    } else {
        damagePerCell = 2;
        pa_te_type = TE_SHIELD_SPARKS;
        damage = (2 * damage) / 3;
    }

    save = power * damagePerCell;
    if (!save) {
        return 0;
    }
    if (save > damage) {
        save = damage;
    }

    SpawnDamage(pa_te_type, point, normal);
    client->powerarmor_framenum = level.framenum + 0.2 * HZ;

    power_used = save / damagePerCell;

    client->inventory[ITEM_CELLS] -= power_used;
    return save;
}

/**
 * How much damage does player's armor absorb?
 */
static int CheckArmor(edict_t *ent, vec3_t point, vec3_t normal, int damage, int te_sparks, int dflags) {
    gclient_t   *client;
    int         save;
    int         index;
    gitem_t     *armor;

    if (!damage) {
        return 0;
    }

    client = ent->client;

    if (!client) {
        return 0;
    }

    if (dflags & DAMAGE_NO_ARMOR) {
        return 0;
    }

    index = ArmorIndex(ent);
    if (!index) {
        return 0;
    }

    armor = INDEX_ITEM(index);

    if (dflags & DAMAGE_ENERGY) {
        save = ceilf(((gitem_armor_t *)armor->info)->energy_protection * damage);
    } else {
        save = ceilf(((gitem_armor_t *)armor->info)->normal_protection * damage);
    }
    if (save >= client->inventory[index]) {
        save = client->inventory[index];
    }

    if (!save) {
        return 0;
    }

    client->inventory[index] -= save;
    SpawnDamage(te_sparks, point, normal);

    return save;
}

/**
 * Should attacker be able to damage targ based on their team status?
 *
 * TODO: implement
 */
static qboolean CheckTeamDamage(edict_t *targ, edict_t *attacker) {
    //FIXME make the next line real and uncomment this block
    // if ((ability to damage a teammate == OFF) && (targ's team == attacker's team))
    return qfalse;
}

/**
 * Apply damage to someone
 *
 * targ        entity that is being damaged
 * inflictor   entity that is causing the damage
 * attacker    entity that caused the inflictor to damage targ
 *   example: targ=monster, inflictor=rocket, attacker=player
 *
 * dir         direction of the attack
 * point       point at which the damage is being inflicted
 * normal      normal vector from that point
 * damage      amount of damage being inflicted
 * knockback   force to be applied against targ as a result of the damage
 *
 * dflags      these flags are used to control how T_Damage works
 *   DAMAGE_RADIUS           damage was indirect (from a nearby explosion)
 *   DAMAGE_NO_ARMOR         armor does not protect from this damage
 *   DAMAGE_ENERGY           damage is from an energy based weapon
 *   DAMAGE_NO_KNOCKBACK     do not affect velocity, just view angles
 *   DAMAGE_BULLET           damage is from a bullet (used for ricochets)
 *   DAMAGE_NO_PROTECTION    kills godmode, armor, everything
 */
void T_Damage(edict_t *targ, edict_t *inflictor, edict_t *attacker, vec3_t dir, vec3_t point, vec3_t normal, int damage, int knockback, int dflags, int mod) {
    gclient_t   *client;
    int         take;
    int         save;
    int         asave;
    int         psave;
    int         te_sparks;

    if (!targ->takedamage) {
        return;
    }

    // possible fragged body
    if (!targ->client) {
        return;
    }

    // friendly fire avoidance
    // if enabled you can't hurt teammates (but you can hurt yourself)
    // knockback still occurs
#if 0
    if ((targ != attacker) && ((int)(dmflags->value) & (DF_MODELTEAMS | DF_SKINTEAMS))) {
        if (OnSameTeam(targ, attacker)) {
            if (DF(NO_FRIENDLY_FIRE))
                damage = 0;
            else
                mod |= MOD_FRIENDLY_FIRE;
        }
    }
#endif
    meansOfDeath = mod;

    client = targ->client;

    if (dflags & DAMAGE_BULLET) {
        te_sparks = TE_BULLET_SPARKS;
    } else {
        te_sparks = TE_SPARKS;
    }

    VectorNormalize(dir);

    if (targ->flags & FL_NO_KNOCKBACK) {
        knockback = 0;
    }

    // figure momentum add
    if (!(dflags & DAMAGE_NO_KNOCKBACK)) {
        if ((knockback) && (targ->movetype != MOVETYPE_NONE) && (targ->movetype != MOVETYPE_BOUNCE) && (targ->movetype != MOVETYPE_PUSH) && (targ->movetype != MOVETYPE_STOP)) {
            float   mass;
            float push;

            if (targ->mass < 50) {
                mass = 50;
            } else {
                mass = targ->mass;
            }

            if (targ->client && attacker == targ) {
                push = 1600.0f * ((float)knockback / mass);
            } else {
                push = 500.0f * ((float)knockback / mass);
            }

            VectorMA(targ->velocity, push, dir, targ->velocity);
        }
    }

    // nothing hurts us
    if (targ == attacker && NOHURT(targ, SELF) && NOHURT(targ, SELF_ARMOR)) {
        return;
    }

    // nothing hurts our teammates
    if (G_Teammates(targ, attacker) && NOHURT(targ, TEAM) && NOHURT(targ, TEAM_ARMOR)) {
        return;
    }

    take = damage;
    save = 0;

    // check for godmode
    if ((targ->flags & FL_GODMODE) && !(dflags & DAMAGE_NO_PROTECTION)) {
        take = 0;
        save = damage;
        SpawnDamage(te_sparks, point, normal);
    }

    // check for invincibility
    if ((client && client->invincible_framenum > level.framenum) && !(dflags & DAMAGE_NO_PROTECTION)) {
        if (targ->pain_debounce_framenum < level.framenum) {
            gi.sound(targ, CHAN_ITEM, gi.soundindex("items/protect4.wav"), 1, ATTN_NORM, 0);
            targ->pain_debounce_framenum = level.framenum + 2 * HZ;
        }
        take = 0;
        save = damage;
    }

    psave = CheckPowerArmor(targ, point, normal, take, dflags);
    take -= psave;

    asave = 0;

    if (client) {
        // check if self damage should take armor away
        if (targ == attacker && !(NOHURT(targ, SELF_ARMOR))) {
            asave = CheckArmor(targ, point, normal, take, te_sparks, dflags);
            take -= asave;
        }

        // check if team damage should take armor away
        if (targ != attacker && G_Teammates(targ, attacker) && !(NOHURT(targ, TEAM_ARMOR))) {
            asave = CheckArmor(targ, point, normal, take, te_sparks, dflags);
            take -= asave;
        }

        // enemy hit, should affect armor
        if (targ != attacker && !G_Teammates(targ, attacker)) {
            asave = CheckArmor(targ, point, normal, take, te_sparks, dflags);
            take -= asave;
        }
    }

    //treat cheat/powerup savings the same as armor
    asave += save;

    // team damage avoidance
    if (!(dflags & DAMAGE_NO_PROTECTION) && CheckTeamDamage(targ, attacker)) {
        return;
    }

    // add to client weapon statistics
    if (attacker->client && targ->client && !targ->deadflag && inflictor != world) {
        G_AccountDamage(targ, inflictor, attacker, take);
    }

    // only adjust scores if we're mid-round. Since scores persist across matches
    // don't make changes based on frags in warmup mode
    if (ARENASTATE(targ) == ARENA_STATE_PLAY || ARENASTATE(targ) == ARENA_STATE_OVERTIME) {
        if (!G_Teammates(attacker, targ)) {
            attacker->client->pers.damage += damage;
            attacker->client->pers.score = attacker->client->pers.damage / 100;
            TEAM(attacker)->damage_dealt += damage;
            TEAM(attacker)->points = (int)(TEAM(attacker)->damage_dealt / 100);
            TEAM(targ)->damage_taken += damage;
        } else {
            attacker->client->pers.damage -= damage;
            attacker->client->pers.score = attacker->client->pers.damage / 100;
            TEAM(attacker)->damage_dealt -= damage;
            TEAM(attacker)->points = (int)(TEAM(attacker)->damage_dealt / 100);
            TEAM(targ)->damage_taken += damage;
        }
    }

    // do the damage
    if (take) {
        if (client) {
            if (targ == attacker) {
                SpawnDamage(TE_BLOOD, targ->s.origin, normal);
            } else {
                SpawnDamage(TE_BLOOD, point, normal);
            }
        } else if (targ == attacker) {
            SpawnDamage(TE_SPARKS, targ->s.origin, normal);
        } else {
            SpawnDamage(TE_SPARKS, point, normal);
        }

        // hurting someone else
        if (targ != attacker) {
            if (!G_Teammates(targ, attacker)) {
                targ->health -= take;
            } else {
                if (!NOHURT(targ, TEAM)) {
                    targ->health -= take;
                }
            }
        } else {
            if (!NOHURT(targ, SELF)) {
                targ->health -= take;
            }
        }

        if (targ->health <= 0 && targ->deadflag != DEAD_DEAD) {
            if (client) {
                targ->flags |= FL_NO_KNOCKBACK;
            }
            Killed(targ, inflictor, attacker, take, point);
            return;
        }
    }

    if (client) {
        if (!(targ->flags & FL_GODMODE) && (take)) {
            targ->pain(targ, attacker, knockback, take);
        }
    } else if (take) {
        if (targ->pain) {
            targ->pain(targ, attacker, knockback, take);
        }
    }

    // add to the damage inflicted on a player this frame
    // the total will be turned into screen blends and view angle kicks
    // at the end of the frame
    if (client) {
        client->damage_parmor += psave;
        client->damage_armor += asave;
        client->damage_blood += take;
        client->damage_knockback += knockback;
        VectorCopy(point, client->damage_from);
    }
}

/**
 * Do some damage based on radius around
 */
void T_RadiusDamage(edict_t *inflictor, edict_t *attacker, float damage, edict_t *ignore, float radius, int mod) {
    float   points;
    edict_t *ent = NULL;
    vec3_t  v;
    vec3_t  dir;

    while ((ent = findradius(ent, inflictor->s.origin, radius)) != NULL) {
        if (ent == ignore) {
            continue;
        }
        if (!ent->takedamage) {
            continue;
        }

        VectorAdd(ent->mins, ent->maxs, v);
        VectorMA(ent->s.origin, 0.5, v, v);
        VectorSubtract(inflictor->s.origin, v, v);
        points = damage - 0.5 * VectorLength(v);
        if (ent == attacker) {
            points = points * 0.5;
        }
        if (points > 0) {
            if (CanDamage(ent, inflictor)) {
                VectorSubtract(ent->s.origin, inflictor->s.origin, dir);
                T_Damage(ent, inflictor, attacker, dir, inflictor->s.origin, vec3_origin, (int)points, (int)points, DAMAGE_RADIUS, mod);
            }
        }
    }
}

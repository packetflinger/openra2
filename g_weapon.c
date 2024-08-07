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
 * This is an internal support routine used for bullet/pellet based weapons.
 */
static void fire_lead(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int te_impact, int hspread, int vspread, int mod) {
    trace_t     tr;
    vec3_t      dir;
    vec3_t      forward, right, up;
    vec3_t      end;
    float       r;
    float       u;
    vec3_t      water_start;
    qboolean    water = qfalse;
    int         content_mask = MASK_SHOT | MASK_WATER;

    tr = gi.trace(self->s.origin, NULL, NULL, start, self, MASK_SHOT);
    if (!(tr.fraction < 1.0)) {
        vectoangles(aimdir, dir);
        AngleVectors(dir, forward, right, up);

        r = crandom() * hspread;
        u = crandom() * vspread;
        VectorMA(start, 8192, forward, end);
        VectorMA(end, r, right, end);
        VectorMA(end, u, up, end);

        if (gi.pointcontents(start) & MASK_WATER) {
            water = qtrue;
            VectorCopy(start, water_start);
            content_mask &= ~MASK_WATER;
        }

        tr = gi.trace(start, NULL, NULL, end, self, content_mask);

        // see if we hit water
        if (tr.contents & MASK_WATER) {
            int     color;

            water = qtrue;
            VectorCopy(tr.endpos, water_start);

            if (!VectorCompare(start, tr.endpos)) {
                if (tr.contents & CONTENTS_WATER) {
                    if (strcmp(tr.surface->name, "*brwater") == 0) {
                        color = SPLASH_BROWN_WATER;
                    } else {
                        color = SPLASH_BLUE_WATER;
                    }
                } else if (tr.contents & CONTENTS_SLIME) {
                    color = SPLASH_SLIME;
                } else if (tr.contents & CONTENTS_LAVA) {
                    color = SPLASH_LAVA;
                } else {
                    color = SPLASH_UNKNOWN;
                }

                if (color != SPLASH_UNKNOWN) {
                    gi.WriteByte(SVC_TEMP_ENTITY);
                    gi.WriteByte(TE_SPLASH);
                    gi.WriteByte(8);
                    gi.WritePosition(tr.endpos);
                    gi.WriteDir(tr.plane.normal);
                    gi.WriteByte(color);
                    gi.multicast(tr.endpos, MULTICAST_PVS);
                }

                // change bullet's course when it enters water
                VectorSubtract(end, start, dir);
                vectoangles(dir, dir);
                AngleVectors(dir, forward, right, up);
                r = crandom() * hspread * 2;
                u = crandom() * vspread * 2;
                VectorMA(water_start, 8192, forward, end);
                VectorMA(end, r, right, end);
                VectorMA(end, u, up, end);
            }

            // re-trace ignoring water this time
            tr = gi.trace(water_start, NULL, NULL, end, self, MASK_SHOT);
        }
    }

    // send gun puff / flash
    if (!((tr.surface) && (tr.surface->flags & SURF_SKY))) {
        if (tr.fraction < 1.0) {
            if (tr.ent->takedamage) {
                T_Damage(tr.ent, self, self, aimdir, tr.endpos, tr.plane.normal, damage, kick, DAMAGE_BULLET, mod);
            } else {
                if (strncmp(tr.surface->name, "sky", 3) != 0) {
                    gi.WriteByte(SVC_TEMP_ENTITY);
                    gi.WriteByte(te_impact);
                    gi.WritePosition(tr.endpos);
                    gi.WriteDir(tr.plane.normal);
                    gi.multicast(tr.endpos, MULTICAST_PVS);
                }
            }
        }
    }

    // if went through water, determine where the end and make a bubble trail
    if (water) {
        vec3_t  pos;

        VectorSubtract(tr.endpos, water_start, dir);
        VectorNormalize(dir);
        VectorMA(tr.endpos, -2, dir, pos);
        if (gi.pointcontents(pos) & MASK_WATER) {
            VectorCopy(pos, tr.endpos);
        } else {
            tr = gi.trace(pos, NULL, NULL, water_start, tr.ent, MASK_WATER);
        }

        VectorAdd(water_start, tr.endpos, pos);
        VectorScale(pos, 0.5, pos);

        gi.WriteByte(SVC_TEMP_ENTITY);
        gi.WriteByte(TE_BUBBLETRAIL);
        gi.WritePosition(water_start);
        gi.WritePosition(tr.endpos);
        gi.multicast(pos, MULTICAST_PVS);
    }
}

/**
 * Fires a single round.  Used for machinegun and chaingun.  Would be fine for
 * pistols, rifles, etc....
 */
void fire_bullet(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int mod) {
    fire_lead(self, start, aimdir, damage, kick, TE_GUNSHOT, hspread, vspread, mod);
}

/**
 * Shoots shotgun pellets.  Used by shotgun and super shotgun.
 */
void fire_shotgun(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int count, int mod) {
    int     i;

    for (i = 0; i < count; i++) {
        fire_lead(self, start, aimdir, damage, kick, TE_SHOTGUN, hspread, vspread, mod);
    }
}

/**
 * Fires a single blaster bolt.  Used by the blaster and hyper blaster.
 */
void blaster_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf) {
    int     mod;

    if (other == self->owner) {
        return;
    }

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(self);
        return;
    }

    if (other->takedamage) {
        if (self->spawnflags & 1) {
            mod = MOD_HYPERBLASTER;
        } else {
            mod = MOD_BLASTER;
        }
        G_BeginDamage();
        T_Damage(other, self, self->owner, self->velocity, self->s.origin, plane->normal, self->dmg, 1, DAMAGE_ENERGY, mod);
        G_EndDamage();
    } else {
        gi.WriteByte(SVC_TEMP_ENTITY);
        gi.WriteByte(TE_BLASTER);
        gi.WritePosition(self->s.origin);
        if (!plane) {
            gi.WriteDir(vec3_origin);
        } else {
            gi.WriteDir(plane->normal);
        }
        gi.multicast(self->s.origin, MULTICAST_PVS);
    }
    G_FreeEdict(self);
}

/**
 *
 */
void fire_blaster(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int effect, qboolean hyper) {
    edict_t *bolt;
    trace_t tr;

    VectorNormalize(dir);

    bolt = G_Spawn();
    bolt->svflags = SVF_DEADMONSTER;
    // yes, I know it looks weird that projectiles are deadmonsters
    // what this means is that when prediction is used against the object
    // (blaster/hyperblaster shots), the player won't be solid clipped against
    // the object.  Right now trying to run into a firing hyperblaster
    // is very jerky since you are predicted 'against' the shots.
    VectorCopy(start, bolt->s.origin);
    VectorCopy(start, bolt->old_origin);
    vectoangles(dir, bolt->s.angles);
    VectorScale(dir, speed, bolt->velocity);
    bolt->movetype = MOVETYPE_FLYMISSILE;
    bolt->clipmask = MASK_SHOT;
    bolt->solid = SOLID_BBOX;
    bolt->flags = FL_NOCLIP_PROJECTILE;
    bolt->s.effects |= effect;
    VectorClear(bolt->mins);
    VectorClear(bolt->maxs);
    bolt->s.modelindex = gi.modelindex("models/objects/laser/tris.md2");
    bolt->s.sound = gi.soundindex("misc/lasfly.wav");
    bolt->owner = self;
    bolt->touch = blaster_touch;
    bolt->nextthink = level.framenum + 2 * HZ;
    bolt->think = G_FreeEdict;
    bolt->dmg = damage;
    bolt->classname = "bolt";
    if (hyper) {
        bolt->spawnflags = 1;
    }
    gi.linkentity(bolt);

    tr = gi.trace(self->s.origin, NULL, NULL, bolt->s.origin, bolt, MASK_SHOT);
    if (tr.fraction < 1.0) {
        VectorMA(bolt->s.origin, -10, dir, bolt->s.origin);
        bolt->touch(bolt, tr.ent, NULL, NULL);
    }
}

/**
 *
 */
static void Grenade_Explode(edict_t *ent) {
    vec3_t      origin;
    int         mod;

    G_BeginDamage();

    //FIXME: if we are onground then raise our Z just a bit since we are a point?
    if (ent->enemy) {
        float   points;
        vec3_t  v;
        vec3_t  dir;

        VectorAdd(ent->enemy->mins, ent->enemy->maxs, v);
        VectorMA(ent->enemy->s.origin, 0.5, v, v);
        VectorSubtract(ent->s.origin, v, v);
        points = ent->dmg - 0.5 * VectorLength(v);
        VectorSubtract(ent->enemy->s.origin, ent->s.origin, dir);
        if (ent->spawnflags & 1) {
            mod = MOD_HANDGRENADE;
        } else {
            mod = MOD_GRENADE;
        }
        T_Damage(ent->enemy, ent, ent->owner, dir, ent->s.origin, vec3_origin, (int)points, (int)points, DAMAGE_RADIUS, mod);
    }

    if (ent->spawnflags & 2) {
        mod = MOD_HELD_GRENADE;
    } else if (ent->spawnflags & 1) {
        mod = MOD_HG_SPLASH;
    } else {
        mod = MOD_G_SPLASH;
    }
    T_RadiusDamage(ent, ent->owner, ent->dmg, ent->enemy, ent->dmg_radius, mod);

    G_EndDamage();

    VectorMA(ent->s.origin, -0.02, ent->velocity, origin);
    gi.WriteByte(SVC_TEMP_ENTITY);
    if (ent->waterlevel) {
        if (ent->groundentity) {
            gi.WriteByte(TE_GRENADE_EXPLOSION_WATER);
        } else {
            gi.WriteByte(TE_ROCKET_EXPLOSION_WATER);
        }
    } else {
        if (ent->groundentity) {
            gi.WriteByte(TE_GRENADE_EXPLOSION);
        } else {
            gi.WriteByte(TE_ROCKET_EXPLOSION);
        }
    }
    gi.WritePosition(origin);
    gi.multicast(ent->s.origin, MULTICAST_PHS);
    G_FreeEdict(ent);
}

/**
 *
 */
static void Grenade_Touch(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf) {
    if (other == ent->owner) {
        return;
    }

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(ent);
        return;
    }

    if (!other->takedamage) {
        if (ent->spawnflags & 1) {
            if (random() > 0.5) {
                gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/hgrenb1a.wav"), 1, ATTN_NORM, 0);
            } else {
                gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/hgrenb2a.wav"), 1, ATTN_NORM, 0);
            }
        } else {
            gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/grenlb1b.wav"), 1, ATTN_NORM, 0);
        }
        return;
    }

    ent->enemy = other;
    Grenade_Explode(ent);
}

/**
 *
 */
void fire_grenade(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int timer, float damage_radius) {
    edict_t *grenade;
    vec3_t  dir;
    vec3_t  forward, right, up;

    vectoangles(aimdir, dir);
    AngleVectors(dir, forward, right, up);

    grenade = G_Spawn();
    VectorCopy(start, grenade->s.origin);
    VectorCopy(start, grenade->old_origin);
    VectorScale(aimdir, speed, grenade->velocity);
    VectorMA(grenade->velocity, 200 + crandom() * 10.0, up, grenade->velocity);
    VectorMA(grenade->velocity, crandom() * 10.0, right, grenade->velocity);
    VectorSet(grenade->avelocity, 300, 300, 300);
    grenade->movetype = MOVETYPE_BOUNCE;
    grenade->clipmask = MASK_SHOT;
    grenade->solid = SOLID_BBOX;
    grenade->s.effects |= EF_GRENADE;
    VectorClear(grenade->mins);
    VectorClear(grenade->maxs);
    grenade->s.modelindex = gi.modelindex("models/objects/grenade/tris.md2");
    grenade->owner = self;
    grenade->touch = Grenade_Touch;
    grenade->nextthink = level.framenum + timer;
    grenade->think = Grenade_Explode;
    grenade->dmg = damage;
    grenade->dmg_radius = damage_radius;
    grenade->classname = "grenade";

    gi.linkentity(grenade);
}

/**
 *
 */
void fire_grenade2(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int timer, float damage_radius, qboolean held) {
    edict_t *grenade;
    vec3_t  dir;
    vec3_t  forward, right, up;

    vectoangles(aimdir, dir);
    AngleVectors(dir, forward, right, up);

    grenade = G_Spawn();
    VectorCopy(start, grenade->s.origin);
    VectorCopy(start, grenade->old_origin);
    VectorScale(aimdir, speed, grenade->velocity);
    VectorMA(grenade->velocity, 200 + crandom() * 10.0, up, grenade->velocity);
    VectorMA(grenade->velocity, crandom() * 10.0, right, grenade->velocity);
    VectorSet(grenade->avelocity, 300, 300, 300);
    grenade->movetype = MOVETYPE_BOUNCE;
    grenade->clipmask = MASK_SHOT;
    grenade->solid = SOLID_BBOX;
    grenade->s.effects |= EF_GRENADE;
    VectorClear(grenade->mins);
    VectorClear(grenade->maxs);
    grenade->s.modelindex = gi.modelindex("models/objects/grenade2/tris.md2");
    grenade->owner = self;
    grenade->touch = Grenade_Touch;
    grenade->nextthink = level.framenum + timer;
    grenade->think = Grenade_Explode;
    grenade->dmg = damage;
    grenade->dmg_radius = damage_radius;
    grenade->classname = "hgrenade";
    if (held) {
        grenade->spawnflags = 3;
    } else {
        grenade->spawnflags = 1;
    }
    grenade->s.sound = gi.soundindex("weapons/hgrenc1b.wav");

    if (timer <= 0) {
        Grenade_Explode(grenade);
    } else {
        gi.sound(self, CHAN_WEAPON, gi.soundindex("weapons/hgrent1a.wav"), 1, ATTN_NORM, 0);
        gi.linkentity(grenade);
    }
}

/**
 *
 */
void rocket_touch(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf) {
    vec3_t      origin;

    if (other == ent->owner) {
        return;
    }

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(ent);
        return;
    }

    // calculate position for the explosion entity
    VectorMA(ent->s.origin, -0.02, ent->velocity, origin);

    G_BeginDamage();

    if (other->takedamage) {
        T_Damage(other, ent, ent->owner, ent->velocity, ent->s.origin, plane->normal, ent->dmg, 0, 0, MOD_ROCKET);
    } else {
        // don't throw any debris in net games
    }

    T_RadiusDamage(ent, ent->owner, ent->radius_dmg, other, ent->dmg_radius, MOD_R_SPLASH);

    G_EndDamage();

    gi.WriteByte(SVC_TEMP_ENTITY);
    if (ent->waterlevel) {
        gi.WriteByte(TE_ROCKET_EXPLOSION_WATER);
    } else {
        gi.WriteByte(TE_ROCKET_EXPLOSION);
    }
    gi.WritePosition(origin);
    gi.multicast(ent->s.origin, MULTICAST_PHS);
    G_FreeEdict(ent);
}

/**
 *
 */
void fire_rocket(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius, int radius_damage) {
    edict_t *rocket;

    rocket = G_Spawn();
    VectorCopy(start, rocket->s.origin);
    VectorCopy(start, rocket->old_origin);
    VectorCopy(dir, rocket->movedir);
    vectoangles(dir, rocket->s.angles);
    VectorScale(dir, speed, rocket->velocity);
    rocket->movetype = MOVETYPE_FLYMISSILE;
    rocket->clipmask = MASK_SHOT;
    rocket->solid = SOLID_BBOX;
    rocket->s.effects |= EF_ROCKET;
    VectorClear(rocket->mins);
    VectorClear(rocket->maxs);
    rocket->s.modelindex = gi.modelindex("models/objects/rocket/tris.md2");
    rocket->owner = self;
    rocket->touch = rocket_touch;
    rocket->nextthink = level.framenum + 8000 * HZ / speed;
    rocket->think = G_FreeEdict;
    rocket->dmg = damage;
    rocket->radius_dmg = radius_damage;
    rocket->dmg_radius = damage_radius;
    rocket->s.sound = gi.soundindex("weapons/rockfly.wav");
    rocket->classname = "rocket";

    gi.linkentity(rocket);
}

/**
 *
 */
void fire_rail(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick) {
    vec3_t      from;
    vec3_t      end;
    trace_t     tr;
    edict_t     *ignore;
    int         mask;
    qboolean    water;
    int i;

    VectorMA(start, 8192, aimdir, end);
    VectorCopy(start, from);
    ignore = self;
    water = qfalse;
    mask = MASK_SHOT | CONTENTS_SLIME | CONTENTS_LAVA;
    for (i = 0; i < 100; i++) {
        tr = gi.trace(from, NULL, NULL, end, ignore, mask);

        if (tr.contents & (CONTENTS_SLIME | CONTENTS_LAVA)) {
            mask &= ~(CONTENTS_SLIME | CONTENTS_LAVA);
            water = qtrue;
        } else {
            //ZOID--added so rail goes through SOLID_BBOX entities (gibs, etc)
            if ((tr.ent->svflags & SVF_MONSTER) || (tr.ent->client) ||
                (tr.ent->solid == SOLID_BBOX)) {
                ignore = tr.ent;
            } else {
                ignore = NULL;
            }

            if ((tr.ent != self) && (tr.ent->takedamage)) {
                G_BeginDamage();
                T_Damage(tr.ent, self, self, aimdir, tr.endpos, tr.plane.normal, damage, kick, 0, MOD_RAILGUN);
                G_EndDamage();
            }
        }

        VectorCopy(tr.endpos, from);
        if (!ignore) {
            break;
        }
    }

    // send gun puff / flash
    gi.WriteByte(SVC_TEMP_ENTITY);
    gi.WriteByte(TE_RAILTRAIL);
    gi.WritePosition(start);
    gi.WritePosition(tr.endpos);
    gi.multicast(self->s.origin, MULTICAST_PHS);
    if (water) {
        gi.WriteByte(SVC_TEMP_ENTITY);
        gi.WriteByte(TE_RAILTRAIL);
        gi.WritePosition(start);
        gi.WritePosition(tr.endpos);
        gi.multicast(tr.endpos, MULTICAST_PHS);
    }
}

/**
 *
 */
void bfg_explode(edict_t *self) {
    edict_t *ent;
    float   points;
    vec3_t  v;
    float   dist;

    if (self->s.frame == 0) {
        // the BFG effect
        ent = NULL;
        while ((ent = findradius(ent, self->s.origin, self->dmg_radius)) != NULL) {
            if (!ent->takedamage) {
                continue;
            }
            if (ent == self->owner) {
                continue;
            }
            if (!CanDamage(ent, self)) {
                continue;
            }
            if (!CanDamage(ent, self->owner)) {
                continue;
            }

            VectorAdd(ent->mins, ent->maxs, v);
            VectorMA(ent->s.origin, 0.5, v, v);
            VectorSubtract(self->s.origin, v, v);
            dist = VectorLength(v);
            points = self->radius_dmg * (1.0 - sqrt(dist / self->dmg_radius));
            if (ent == self->owner) {
                points = points * 0.5;
            }

            gi.WriteByte(SVC_TEMP_ENTITY);
            gi.WriteByte(TE_BFG_EXPLOSION);
            gi.WritePosition(ent->s.origin);
            gi.multicast(ent->s.origin, MULTICAST_PHS);
            G_BeginDamage();
            T_Damage(ent, self, self->owner, self->velocity, ent->s.origin, vec3_origin, (int)points, 0, DAMAGE_ENERGY, MOD_BFG_EFFECT);
            G_EndDamage();
        }
    }

    self->nextthink = level.framenum + FRAMEDIV;
    self->s.frame++;
    if (self->s.frame == 5) {
        self->think = G_FreeEdict;
    }
}

/**
 *
 */
void bfg_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf) {
    if (other == self->owner) {
        return;
    }

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(self);
        return;
    }

    G_BeginDamage();

    // core explosion - prevents firing it into the wall/floor
    if (other->takedamage) {
        T_Damage(other, self, self->owner, self->velocity, self->s.origin, plane->normal, 200, 0, 0, MOD_BFG_BLAST);
    }
    T_RadiusDamage(self, self->owner, 200, other, 100, MOD_BFG_BLAST);
    G_EndDamage();

    gi.sound(self, CHAN_VOICE, gi.soundindex("weapons/bfg__x1b.wav"), 1, ATTN_NORM, 0);
    self->solid = SOLID_NOT;
    self->touch = NULL;
    VectorMA(self->s.origin, -1 * FRAMETIME, self->velocity, self->s.origin);
    VectorClear(self->velocity);
    self->s.modelindex = gi.modelindex("sprites/s_bfg3.sp2");
    self->s.frame = 0;
    self->s.sound = 0;
    self->s.effects &= ~EF_ANIM_ALLFAST;
    NEXT_KEYFRAME(self, bfg_explode);
    self->enemy = other;

    gi.WriteByte(SVC_TEMP_ENTITY);
    gi.WriteByte(TE_BFG_BIGEXPLOSION);
    gi.WritePosition(self->s.origin);
    gi.multicast(self->s.origin, MULTICAST_PVS);
}

/**
 *
 */
void bfg_think(edict_t *self) {
    edict_t *ent;
    edict_t *ignore;
    vec3_t  point;
    vec3_t  dir;
    vec3_t  start;
    vec3_t  end;
    int     dmg;
    trace_t tr;

    dmg = 5;

    ent = NULL;
    while ((ent = findradius(ent, self->s.origin, 256)) != NULL) {
        if (ent == self) {
            continue;
        }

        if (ent == self->owner) {
            continue;
        }

        if (!ent->takedamage) {
            continue;
        }

        if (!(ent->svflags & SVF_MONSTER) && (!ent->client) && (strcmp(ent->classname, "misc_explobox") != 0)) {
            continue;
        }

        VectorMA(ent->absmin, 0.5, ent->size, point);

        VectorSubtract(point, self->s.origin, dir);
        VectorNormalize(dir);

        ignore = self;
        VectorCopy(self->s.origin, start);
        VectorMA(start, 2048, dir, end);
        while (1) {
            tr = gi.trace(start, NULL, NULL, end, ignore, CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_DEADMONSTER);

            if (!tr.ent) {
                break;
            }

            // hurt it if we can
            if ((tr.ent->takedamage) && !(tr.ent->flags & FL_IMMUNE_LASER) && (tr.ent != self->owner)) {
                G_BeginDamage();
                T_Damage(tr.ent, self, self->owner, dir, tr.endpos, vec3_origin, dmg, 1, DAMAGE_ENERGY, MOD_BFG_LASER);
                G_EndDamage();
            }

            // if we hit something that's not a monster or player we're done
            if (!(tr.ent->svflags & SVF_MONSTER) && (!tr.ent->client)) {
                gi.WriteByte(SVC_TEMP_ENTITY);
                gi.WriteByte(TE_LASER_SPARKS);
                gi.WriteByte(4);
                gi.WritePosition(tr.endpos);
                gi.WriteDir(tr.plane.normal);
                gi.WriteByte(self->s.skinnum);
                gi.multicast(tr.endpos, MULTICAST_PVS);
                break;
            }

            ignore = tr.ent;
            VectorCopy(tr.endpos, start);
        }

        gi.WriteByte(SVC_TEMP_ENTITY);
        gi.WriteByte(TE_BFG_LASER);
        gi.WritePosition(self->s.origin);
        gi.WritePosition(tr.endpos);
        gi.multicast(self->s.origin, MULTICAST_PHS);
    }
    self->nextthink = level.framenum + FRAMEDIV;
}

/**
 *
 */
void fire_bfg(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius) {
    edict_t *bfg;

    bfg = G_Spawn();
    VectorCopy(start, bfg->s.origin);
    VectorCopy(start, bfg->old_origin);
    VectorCopy(dir, bfg->movedir);
    vectoangles(dir, bfg->s.angles);
    VectorScale(dir, speed, bfg->velocity);
    bfg->movetype = MOVETYPE_FLYMISSILE;
    bfg->clipmask = MASK_SHOT;
    bfg->solid = SOLID_BBOX;
    bfg->s.effects |= EF_BFG | EF_ANIM_ALLFAST;
    VectorClear(bfg->mins);
    VectorClear(bfg->maxs);
    bfg->s.modelindex = gi.modelindex("sprites/s_bfg1.sp2");
    bfg->owner = self;
    bfg->touch = bfg_touch;
    NEXT_KEYFRAME(bfg, bfg_think);
    bfg->radius_dmg = damage;
    bfg->dmg_radius = damage_radius;
    bfg->classname = "bfg blast";
    bfg->s.sound = gi.soundindex("weapons/bfg__l1a.wav");
    bfg->teammaster = bfg;
    bfg->teamchain = NULL;

    gi.linkentity(bfg);
}

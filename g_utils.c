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
// g_utils.c -- misc utility functions for game module

#include "g_local.h"

/**
 *
 */
void G_ProjectSource(vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result) {
    result[0] = point[0] + forward[0] * distance[0] + right[0] * distance[1];
    result[1] = point[1] + forward[1] * distance[0] + right[1] * distance[1];
    result[2] = point[2] + forward[2] * distance[0] + right[2] * distance[1] + distance[2];
}

/**
 * Searches all active entities for the next one that holds the matching
 * string at fieldofs (use the FOFS() macro) in the structure.
 *
 * Searches beginning at the edict after from, or the beginning if NULL
 * NULL will be returned if the end of the list is reached.
 */
edict_t *G_Find(edict_t *from, size_t fieldofs, char *match) {
    char    *s;

    if (!from) {
        from = g_edicts;
    } else {
        from++;
    }

    for (; from < &g_edicts[globals.num_edicts]; from++) {
        if (!from->inuse) {
            continue;
        }
        s = *(char **)((byte *)from + fieldofs);
        if (!s) {
            continue;
        }
        if (!Q_stricmp(s, match)) {
            return from;
        }
    }

    return NULL;
}

/**
 * Returns entities that have origins within a spherical area
 */
edict_t *findradius(edict_t *from, vec3_t org, float rad) {
    vec3_t  eorg;
    int     j;

    if (!from) {
        from = g_edicts;
    } else {
        from++;
    }
    for (; from < &g_edicts[globals.num_edicts]; from++) {
        if (!from->inuse) {
            continue;
        }
        if (from->solid == SOLID_NOT) {
            continue;
        }
        for (j = 0; j < 3; j++) {
            eorg[j] = org[j] - (from->s.origin[j] + (from->mins[j] + from->maxs[j]) * 0.5);
        }
        if (VectorLength(eorg) > rad) {
            continue;
        }
        return from;
    }
    return NULL;
}

#define MAXCHOICES  8

/**
 * Searches all active entities for the next one that holds the matching string
 * at fieldofs (use the FOFS() macro) in the structure.
 *
 * Searches beginning at the edict after from, or the beginning if NULL
 * NULL will be returned if the end of the list is reached.
 */
edict_t *G_PickTarget(char *targetname) {
    edict_t *ent = NULL;
    int     num_choices = 0;
    edict_t *choice[MAXCHOICES];

    if (!targetname) {
        gi.dprintf("G_PickTarget called with NULL targetname\n");
        return NULL;
    }

    while (1) {
        ent = G_Find(ent, FOFS(targetname), targetname);
        if (!ent) {
            break;
        }
        choice[num_choices++] = ent;
        if (num_choices == MAXCHOICES) {
            break;
        }
    }
    if (!num_choices) {
        gi.dprintf("G_PickTarget: target %s not found\n", targetname);
        return NULL;
    }
    return choice[rand_byte() % num_choices];
}

/**
 *
 */
void Think_Delay(edict_t *ent) {
    G_UseTargets(ent, ent->activator);
    G_FreeEdict(ent);
}

/**
 * The global "activator" should be set to the entity that initiated the firing.
 *
 * If self.delay is set, a DelayedUse entity will be created that will actually
 * do the SUB_UseTargets after that many seconds have passed.
 *
 * Centerprints any self.message to the activator.
 *
 * Search for (string)targetname in all entities that match (string)self.target
 * and call their .use function
 */
void G_UseTargets(edict_t *ent, edict_t *activator) {
    edict_t     *t;

    if (ent->delay) {
        // create a temp object to fire at a later time
        t = G_Spawn();
        t->classname = "DelayedUse";
        t->nextthink = level.framenum + ent->delay * HZ;
        t->think = Think_Delay;
        t->activator = activator;
        if (!activator) {
            gi.dprintf("Think_Delay with no activator\n");
        }
        t->message = ent->message;
        t->target = ent->target;
        t->killtarget = ent->killtarget;
        return;
    }

    // the print message
    if ((ent->message) && !(activator->svflags & SVF_MONSTER)) {
        gi.centerprintf(activator, "%s", ent->message);
        if (ent->noise_index) {
            gi.sound(activator, CHAN_AUTO, ent->noise_index, 1, ATTN_NORM, 0);
        } else {
            gi.sound(activator, CHAN_AUTO, gi.soundindex("misc/talk1.wav"), 1, ATTN_NORM, 0);
        }
    }

    if (ent->killtarget) {
        t = NULL;
        while ((t = G_Find(t, FOFS(targetname), ent->killtarget))) {
            G_FreeEdict(t);
            if (!ent->inuse) {
                gi.dprintf("entity was removed while using killtargets\n");
                return;
            }
        }
    }

    if (ent->target) {
        t = NULL;
        while ((t = G_Find(t, FOFS(targetname), ent->target))) {
            // doors fire area portals in a specific way
            if (!Q_stricmp(t->classname, "func_areaportal") &&
                (!Q_stricmp(ent->classname, "func_door") || !Q_stricmp(ent->classname, "func_door_rotating"))) {
                continue;
            }

            if (t == ent) {
                gi.dprintf("WARNING: Entity used itself.\n");
            } else {
                if (t->use) {
                    t->use(t, ent, activator);
                }
            }
            if (!ent->inuse) {
                gi.dprintf("entity was removed while using targets\n");
                return;
            }
        }
    }
}

/**
 * This is just a convenience function for making temporary vectors for
 * function calls
 */
float *tv(float x, float y, float z) {
    static  int     index;
    static  vec3_t  vecs[8];
    float   *v;

    // use an array so that multiple tempvectors won't collide
    // for a while
    v = vecs[index];
    index = (index + 1) & 7;

    v[0] = x;
    v[1] = y;
    v[2] = z;

    return v;
}

/**
 * This is just a convenience function for printing vectors
 */
char *vtos(vec3_t v) {
    static  int     index;
    static  char    str[8][32];
    char    *s;

    // use an array so that multiple vtos won't collide
    s = str[index];
    index = (index + 1) & 7;
    Q_snprintf(s, 32, "(%i %i %i)", (int)v[0], (int)v[1], (int)v[2]);
    return s;
}

vec3_t VEC_UP       = {0, -1, 0};
vec3_t MOVEDIR_UP   = {0, 0, 1};
vec3_t VEC_DOWN     = {0, -2, 0};
vec3_t MOVEDIR_DOWN = {0, 0, -1};

/**
 *
 */
void G_SetMovedir(vec3_t angles, vec3_t movedir) {
    if (VectorCompare(angles, VEC_UP)) {
        VectorCopy(MOVEDIR_UP, movedir);
    } else if (VectorCompare(angles, VEC_DOWN)) {
        VectorCopy(MOVEDIR_DOWN, movedir);
    } else {
        AngleVectors(angles, movedir, NULL, NULL);
    }

    VectorClear(angles);
}

/**
 *
 */
float vectoyaw(vec3_t vec) {
    float   yaw;

    if (/*vec[YAW] == 0 &&*/ vec[PITCH] == 0) {
        yaw = 0;
        if (vec[YAW] > 0) {
            yaw = 90;
        } else if (vec[YAW] < 0) {
            yaw = -90;
        }
    } else {
        yaw = (int)(atan2(vec[YAW], vec[PITCH]) * 180 / M_PI);
        if (yaw < 0) {
            yaw += 360;
        }
    }
    return yaw;
}

/**
 *
 */
void vectoangles(vec3_t value1, vec3_t angles) {
    float   forward;
    float   yaw, pitch;

    if (value1[1] == 0 && value1[0] == 0) {
        yaw = 0;
        if (value1[2] > 0) {
            pitch = 90;
        } else {
            pitch = 270;
        }
    } else {
        if (value1[0]) {
            yaw = (int)(atan2(value1[1], value1[0]) * 180 / M_PI);
        } else if (value1[1] > 0) {
            yaw = 90;
        } else {
            yaw = -90;
        }
        if (yaw < 0) {
            yaw += 360;
        }

        forward = sqrt(value1[0] * value1[0] + value1[1] * value1[1]);
        pitch = (int)(atan2(value1[2], forward) * 180 / M_PI);
        if (pitch < 0) {
            pitch += 360;
        }
    }

    angles[PITCH] = -pitch;
    angles[YAW] = yaw;
    angles[ROLL] = 0;
}

/**
 * Return a newly allocated copy of a string
 */
char *G_CopyString(const char *in) {
    size_t  len;

    if (!in) {
        return NULL;
    }
    len = strlen(in) + 1;
    return memcpy(G_Malloc(len), in, len);
}

/**
 * Initialize an entity
 */
void G_InitEdict(edict_t *e) {
    e->inuse = qtrue;
    e->classname = "noclass";
    e->gravity = 1.0;
    e->s.number = e - g_edicts;
}

/**
 * Either finds a free edict, or allocates a new one. Try to avoid reusing an
 * entity that was recently freed, because it can cause the client to think the
 * entity morphed into something else instead of being removed and recreated,
 * which can cause interpolated angles and bad trails.
 */
edict_t *G_Spawn(void) {
    int         i;
    edict_t     *e;

    e = &g_edicts[game.maxclients + 1];
    for (i = game.maxclients + 1; i < globals.num_edicts; i++, e++) {
        // the first couple seconds of server time can involve a lot of
        // freeing and allocating, so relax the replacement policy
        if (!e->inuse && (e->freetime < 2 || level.time - e->freetime > 0.5)) {
            G_InitEdict(e);
            return e;
        }
    }

    if (i == game.maxentities) {
        gi.error("ED_Alloc: no free edicts");
    }

    globals.num_edicts++;
    G_InitEdict(e);
    return e;
}

/**
 * Marks the edict as free
 */
void G_FreeEdict(edict_t *ed) {
    gi.unlinkentity(ed);        // unlink from world

    if ((ed - g_edicts) <= (maxclients->value + BODY_QUEUE_SIZE)) {
        return;
    }
    memset(ed, 0, sizeof(*ed));
    ed->classname = "freed";
    ed->freetime = level.time;
    ed->inuse = qfalse;
}

/**
 *
 */
void G_TouchTriggers(edict_t *ent) {
    int         i, num;
    edict_t     *touch[MAX_EDICTS], *hit;

    // dead things don't activate triggers!
    if ((ent->client || (ent->svflags & SVF_MONSTER)) && (ent->health <= 0)) {
        return;
    }

    num = gi.BoxEdicts(ent->absmin, ent->absmax, touch,
                       MAX_EDICTS, AREA_TRIGGERS);

    // be careful, it is possible to have an entity in this
    // list removed before we get to it (killtriggered)
    for (i = 0; i < num; i++) {
        hit = touch[i];
        if (!hit->inuse) {
            continue;
        }
        if (!hit->touch) {
            continue;
        }
        hit->touch(hit, ent, NULL, NULL);
    }
}

/**
 * Call after linking a new trigger in during gameplay to force all entities it
 * covers to immediately touch it
 */
#if 0
void G_TouchSolids(edict_t *ent) {
    int         i, num;
    edict_t     *touch[MAX_EDICTS], *hit;

    num = gi.BoxEdicts(ent->absmin, ent->absmax, touch,
                       MAX_EDICTS, AREA_SOLID);

    // be careful, it is possible to have an entity in this
    // list removed before we get to it (killtriggered)
    for (i = 0; i < num; i++) {
        hit = touch[i];
        if (!hit->inuse) {
            continue;
        }
        if (ent->touch) {
            ent->touch(hit, ent, NULL, NULL);
        }
        if (!ent->inuse) {
            break;
        }
    }
}
#endif

/**
 *
 */
void G_ShuffleArray(void *base, size_t n) {
    size_t i, j;
    void *temp, **array;

    if (n < 2) {
        return;
    }
    array = base;
    for (i = n - 1; i > 0; i--) {
        j = rand_byte() % (i + 1);
        temp = array[j];
        array[j] = array[i];
        array[i] = temp;
    }
}

/**
 * Convert text to console characters
 */
size_t G_HighlightStr(char *dst, const char *src, size_t size) {
    size_t ret = strlen(src);

    if (size) {
        size_t i, len = ret >= size ? size - 1 : ret;

        for (i = 0; i < len; i++) {
            dst[i] = src[i] | 0x80;
        }
        dst[i] = 0;
    }
    return ret;
}

/**
 * Kills all entities that would touch the proposed new positioning of ent. Ent
 * should be unlinked before calling this!
 */
qboolean G_KillBox(edict_t *ent) {
    if (level.intermission_framenum) {
        return qfalse;
    }

    if ((int)g_bugs->value < 2) {
        edict_t *touch[MAX_EDICTS];
        int     count;
        int     i;

        //r1: much more vicious telefrag code, nukes anything that isn't ent
        count = gi.BoxEdicts(ent->absmin, ent->absmax, touch, MAX_EDICTS, AREA_SOLID);
        for (i = 0; i < count; i++) {
            if (touch[i] == ent) {
                continue;
            }

            //no point killing anything that won't clip (eg corpses)
            if (touch[i]->solid == SOLID_NOT || (touch[i]->svflags & SVF_DEADMONSTER)) {
                continue;
            }

            if (touch[i]->inuse) {
                T_Damage(touch[i], ent, ent, vec3_origin, ent->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MOD_TELEFRAG);
            }
        }
    } else {
        trace_t     tr;

        while (1) {
            tr = gi.trace(ent->s.origin, ent->mins, ent->maxs, ent->s.origin, ent, MASK_PLAYERSOLID);
            if (!tr.ent) {
                break;
            }

            // nail it
            T_Damage(tr.ent, ent, ent, vec3_origin, ent->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MOD_TELEFRAG);

            // if we didn't kill it, fail
            if (tr.ent->solid) {
                return qfalse;
            }
        }
    }
    return qtrue;        // all clear
}

/**
 * level.arenas array isn't indexed with arena numbers...
 */
int ArenaIndex(int arena_num) {
    int i;
    for (i=0; i<level.arena_count; i++) {
        if (level.arenas[i].number == arena_num) {
            return i;
        }
    }
    return 0;
}

/**
 * Compare strings with wildcards
 */
qboolean match(char *pattern, char *haystack) {
    if (*pattern == '\0' && *haystack == '\0') {
        return true;
    }

    if (*pattern == '*' && *(pattern+1) != '\0' && *haystack == '\0') {
        return false;
    }

    if (*pattern == '?' || *pattern == *haystack) {
        return match(pattern+1, haystack+1);
    }

    if (*pattern == '*') {
        return match(pattern+1, haystack) || match(pattern, haystack+1);
    }

    return false;
}

/**
 * Break a key/value string into an array of it's parts.
 *
 * input is the input string
 * delimiter is the character between the key and value
 * len is the size to allocate for each the key and value
 *
 * Returned pointern needs to be free()'d
 */
char **KeyValueSplit(char *input, char *delimiter, size_t len) {
    char **out;
    uint8_t i = 0;
    char *start;

    out = malloc(2 * sizeof(char *));
    out[0] = malloc(len * sizeof(char *));
    out[1] = malloc(len * sizeof(char *));

    memset(out, 0, 2);
    memset(out[0], 0, len);
    memset(out[1], 0, len);

    start = input;
    while (*input) {
        if (*input == *delimiter) {
            memcpy(out[i], start, input - start);
            start = input + 1;
            i++;
        }
        input++;
    }
    memcpy(out[i], start, input - start);
    return out;
}

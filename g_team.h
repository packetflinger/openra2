
#include "g_local.h"

#pragma once

arena_team_t *FindTeam(arena_t *a, arena_team_type_t type);
void G_TeamJoin(edict_t *ent, arena_team_type_t type, qboolean forced);
void G_TeamPart(edict_t *ent, qboolean silent);

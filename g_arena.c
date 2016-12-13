/*
Copyright (C) 2017 Packetflinger.com

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


arena_t *FindArena(edict_t *ent) {

	if (!ent->client)  
		return NULL;
	
	return ent->client->pers.arena_p;
}

static arena_team_t *FindTeam(edict_t *ent, arena_team_type_t type) {
	
	arena_t *a = FindArena(ent);
	
	if (!a) {
		return NULL;
	}
	
	if (a->team_home.type == type)
		return &(a->team_home);
	
	if (a->team_away.type == type)
		return &(a->team_away);
	
	return NULL;
}

void change_arena(edict_t *self) {
	
	PutClientInServer(self);
	
	// add a teleportation effect
    self->s.event = EV_PLAYER_TELEPORT;

    // hold in place briefly
    self->client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
    self->client->ps.pmove.pm_time = 14;

    self->client->respawn_framenum = level.framenum;
}

// broadcast print to only members of specified arena
void G_bprintf(arena_t *arena, int level, const char *fmt, ...) {
	va_list     argptr;
    char        string[MAX_STRING_CHARS];
    size_t      len;
    int         i;
	edict_t		*other;
	
    va_start(argptr, fmt);
    len = Q_vsnprintf(string, sizeof(string), fmt, argptr);
    va_end(argptr);

	
    if (len >= sizeof(string)) {
        return;
    }
	
	for (i = 1; i <= game.maxclients; i++) {
        other = &g_edicts[i];
		
        if (!other->inuse)
            continue;
		
        if (!other->client)
            continue;
		
        if (arena != other->client->pers.arena_p)
			continue;
		
        gi.cprintf(other, level, "%s\n", string);
    }
}

// give the player all the items/weapons they need
void G_GiveItems(edict_t *ent) {
	
	int flags;
	flags = level.map->arenas[ent->client->pers.arena].weapon_flags;
	
	if (flags < 2)
		flags = ARENAWEAPON_ALL;
		
	// weapons
	if (flags & ARENAWEAPON_SHOTGUN)
		ent->client->inventory[ITEM_SHOTGUN] = 1;
	
	if (flags & ARENAWEAPON_SUPERSHOTGUN)
		ent->client->inventory[ITEM_SUPERSHOTGUN] = 1;
	
	if (flags & ARENAWEAPON_MACHINEGUN)
		ent->client->inventory[ITEM_MACHINEGUN] = 1;
	
	if (flags & ARENAWEAPON_CHAINGUN)
		ent->client->inventory[ITEM_CHAINGUN] = 1;
	
	if (flags & ARENAWEAPON_GRENADELAUNCHER)
		ent->client->inventory[ITEM_GRENADELAUNCHER] = 1;
	
	if (flags & ARENAWEAPON_HYPERBLASTER)
		ent->client->inventory[ITEM_HYPERBLASTER] = 1;
	
	if (flags & ARENAWEAPON_ROCKETLAUNCHER)
		ent->client->inventory[ITEM_ROCKETLAUNCHER] = 1;
	
	if (flags & ARENAWEAPON_RAILGUN)
		ent->client->inventory[ITEM_RAILGUN] = 1;
	
	if (flags & ARENAWEAPON_BFG)
		ent->client->inventory[ITEM_BFG] = 1;
	
	// ammo
	ent->client->inventory[ITEM_SLUGS] = 25;
	ent->client->inventory[ITEM_ROCKETS] = 30;
	ent->client->inventory[ITEM_CELLS] = 150;
	ent->client->inventory[ITEM_GRENADES] = 20;
	ent->client->inventory[ITEM_BULLETS] = 200;
	ent->client->inventory[ITEM_SHELLS] = 50;
	
	// armor
	ent->client->inventory[ITEM_ARMOR_BODY] = 110;
}


void G_JoinTeam(edict_t *ent, arena_team_type_t type) {
	
	if (!ent->client)
		return;
	
	arena_t *arena = ent->client->pers.arena_p;
	arena_team_t *team = FindTeam(ent, type);
	
	if (!arena) {
		gi.cprintf(ent, PRINT_HIGH, "Unknown arena\n");
		return;
	}
	
	if (!team) {
		gi.cprintf(ent, PRINT_HIGH, "Unknown team, can't join it\n");
		return;
	}
	
	// match has started, cant join
	if (arena->state >= ARENA_STATE_PLAY) {
		gi.cprintf(ent, PRINT_HIGH, "Match in progress, you can't join now\n");
		return;
	}
	
	//already on that team
	if (ent->client->pers.team) {
		if (ent->client->pers.team == team) {
			gi.cprintf(ent, PRINT_HIGH, "You're already on that team dumbass!\n");
			return;
		}
		
		G_PartTeam(ent);
	}
	
	// add player to the team
	ent->client->pers.team = team;
	team->player_count++;

	int i;
	for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
		if (!team->players[i]) {	// free player slot, take it
			team->players[i] = ent;
			break;
		}
	}
	
	if (!team->captain) {
		team->captain = ent;
	}
	
	G_bprintf(arena, PRINT_HIGH, "%s joined %s\n", ent->client->pers.netname, team->name);
	
	// force the skin
	G_SetSkin(ent, team->skin);
	
	// throw them into the game
	spectator_respawn(ent, CONN_SPAWNED);
}

// remove this player from whatever team they're on
void G_PartTeam(edict_t *ent) {
	
	arena_team_t *oldteam;
	
	if (!ent->client)
		return;
	
	oldteam = ent->client->pers.team;
	
	if (!oldteam)
		return;
	
	gi.cprintf(ent, PRINT_HIGH, "Removing you from %s\n", oldteam->name);
	
	oldteam->player_count--;
	if (oldteam->captain == ent) {
		oldteam->captain = 0;
	}
	
	int i;
	for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
		if (oldteam->players[i] == ent) {
			oldteam->players[i] = NULL;
			break;
		}
	}
	
	ent->client->pers.team = 0;
}

void G_SetSkin(edict_t *ent, const char *skin) {
	
	if (!ent->client) {
		return;
	}
	
	edict_t *e;
	
	for (e = g_edicts + 1; e <= g_edicts + game.maxclients; e++) {	
		if (!e->inuse)
			continue;
		
		gi.WriteByte(svc_configstring);
		gi.WriteShort(CS_PLAYERSKINS + (ent - g_edicts) - 1);
		gi.WriteString(va("%s\\%s", ent->client->pers.netname, skin));
		gi.unicast(e, true);
	}
}

// switches the player's gun-in-hand after spawning
void G_StartingWeapon(edict_t *ent, int gun) {
	if (!ent->client)
		return;
	
	if (ent->client->inventory[gun]) {
		
	}
}

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

// check for things like state changes, start/end of rounds, countdown clocks, etc
void G_ArenaThink(arena_t *a) {
	static qboolean foundwinner = false;
	
	if (!a)
		return;

	if (a->state == ARENA_STATE_TIMEOUT) {
		G_TimeoutFrame(a);
		return;
	}
	
	// end of round
	if (a->state == ARENA_STATE_PLAY) {

		arena_team_t *winner = G_GetWinningTeam(a);
		
		if (winner && !foundwinner) {
			a->round_end_frame = level.framenum + SECS_TO_FRAMES(2);
			foundwinner = true;
		}
		
		if (a->round_end_frame == level.framenum) {
			foundwinner = false;
			G_EndRound(a, winner);
			return;
		}
	}
	
	if (a->state == ARENA_STATE_COUNTDOWN) {
		//gi.dprintf("(A%d) Round %d starting in %s\n", a->number, a->current_round, "never");
		// refill health ammo
		// prevent firing
	}
	
	// start a round
	if (a->state < ARENA_STATE_PLAY && a->round_start_frame) {
		int framesleft = a->round_start_frame - level.framenum;
		
		if (framesleft > 0 && framesleft % SECS_TO_FRAMES(1) == 0) {
			
			G_bprintf(a, PRINT_HIGH, "%d\n", (int)(framesleft / HZ));
			
		} else if (framesleft == 0) {
			
			G_StartRound(a);
			return;
		}
	}
	
	// pregame
	if (a->state == ARENA_STATE_WARMUP) {
		if (G_CheckReady(a)) {	// is everyone ready?
			a->state = ARENA_STATE_COUNTDOWN;
			a->current_round = 1;
			a->round_start_frame = level.framenum + SECS_TO_FRAMES(10);
			
			G_RespawnPlayers(a);
		}
	}
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
		
        gi.cprintf(other, level, "%s", string);
    }
}

// see if all players are ready
qboolean G_CheckReady(arena_t *a) {
	qboolean ready_home = false;
	qboolean ready_away = false;
	
	int i;
	for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
		if (a->team_home.players[i]) {
			if (!a->team_home.players[i]->client->pers.ready) {
				ready_home = false;
				break;
			} else {
				ready_home = true;
			}
		}
	}
	
	for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
		if (a->team_away.players[i]) {
			if (!a->team_away.players[i]->client->pers.ready) {
				ready_away = false;
				break;
			} else {
				ready_away = true;
			}
		}
	}
	
	return ready_home && ready_away;
}

// match countdowns...
void G_CheckTime(arena_t *a) {
	
	
}

void G_ForceReady(arena_team_t *team, qboolean ready) {
	
	int i;
	for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
		if (team->players[i]) {
			team->players[i]->client->pers.ready = ready;
		}
	}
}

void G_EndMatch(arena_t *a) {
	G_bprintf(a, PRINT_HIGH, "Match finished\n", a->current_round);
	
	a->state = ARENA_STATE_WARMUP;
	
	G_ForceReady(&a->team_home, false);
	G_ForceReady(&a->team_away, false);
}

void G_EndRound(arena_t *a, arena_team_t *winner) {
	a->round_start_frame = 0;
	G_bprintf(a, PRINT_HIGH, "%s won round %d/%d!\n", winner->name, a->current_round, a->round_limit);
	
	if (a->current_round == a->round_limit) {
		G_EndMatch(a);
		return;
	}
	
	a->current_round++;
	
	a->state = ARENA_STATE_COUNTDOWN;
	a->round_start_frame = level.framenum + SECS_TO_FRAMES(10);
	
	a->round_end_frame = 0;
	G_RespawnPlayers(a);
}


void G_FreezePlayers(arena_t *a, qboolean freeze) {
	
	if (!a)
		return;
	
	pmtype_t type;
	if (freeze) {
		type = PM_FREEZE;
	} else {
		type = PM_NORMAL;
	}
		
	int i;
	for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
		if (a->team_home.players[i]) {
			a->team_home.players[i]->client->ps.pmove.pm_type = type;
		}
		
		if (a->team_away.players[i]) {
			a->team_away.players[i]->client->ps.pmove.pm_type = type;
		}
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

// give back all the ammo, health and armor for start of a round
void G_RefillInventory(edict_t *ent) {
	// ammo
	ent->client->inventory[ITEM_SLUGS] = 25;
	ent->client->inventory[ITEM_ROCKETS] = 30;
	ent->client->inventory[ITEM_CELLS] = 150;
	ent->client->inventory[ITEM_GRENADES] = 20;
	ent->client->inventory[ITEM_BULLETS] = 200;
	ent->client->inventory[ITEM_SHELLS] = 50;
	
	// armor
	ent->client->inventory[ITEM_ARMOR_BODY] = 110;
	
	// health
	ent->health = 100;
}

// respawn all players resetting their inventory
void G_RespawnPlayers(arena_t *a) {
	
	int i;
	edict_t *ent;
	
	for (i=0; i<MAX_ARENA_TEAM_PLAYERS; i++) {
		
		ent = a->team_home.players[i];
		if (ent && ent->inuse) {
			G_RefillInventory(ent);
			spectator_respawn(ent, CONN_SPAWNED);
			G_StartingWeapon(ent);
		}
		
		ent = a->team_away.players[i];
		if (ent && ent->inuse) {
			G_RefillInventory(ent);
			spectator_respawn(ent, CONN_SPAWNED);
			G_StartingWeapon(ent);
		}
		
		a->team_home.players_alive = a->team_home.player_count;
		a->team_away.players_alive = a->team_away.player_count;
	}
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

void G_StartRound(arena_t *a) {
	
	a->team_home.players_alive = a->team_home.player_count;
	a->team_away.players_alive = a->team_away.player_count;
	
	a->state = ARENA_STATE_PLAY;
	G_bprintf(a, PRINT_HIGH, "Fight!\n");
}

// switches the player's gun-in-hand after spawning
void G_StartingWeapon(edict_t *ent) {
	if (!ent->client)
		return;
	
	ent->client->newweapon = FindItem("rocket launcher");
	ChangeWeapon(ent);
}

// find the team this guy is on and record the death
void G_TeamMemberDeath(edict_t *ent) {
	
	int i = 0;
	arena_team_t *team = ent->client->pers.team;
	
	if (i && team)
		return;

}

// do stuff if this arena is currently in a timeout
void G_TimeoutFrame(arena_t *a) {
	
	// expired
	if (a->timein_frame == level.framenum) {
		a->state = ARENA_STATE_PLAY;
		a->timeout_frame = 0;
		a->timein_frame = 0;
		a->timeout_caller = NULL;
		return;
	}
	
	// countdown
	if (a->timein_frame <= level.framenum + SECS_TO_FRAMES(5)) {
		int framesleft = a->timein_frame - level.framenum;
		
		if (framesleft > 0 && framesleft % SECS_TO_FRAMES(1) == 0) {
			G_bprintf(a, PRINT_HIGH, "%d\n", (int)(framesleft / HZ));
		}
	}
}

// 
arena_team_t *G_GetWinningTeam(arena_t *a) {
	
	if (!a)
		return NULL;
	
	if (a->team_home.players_alive == 0)
		return &(a->team_away);
	
	if (a->team_away.players_alive == 0)
		return &(a->team_home);
	
	return NULL;
}	

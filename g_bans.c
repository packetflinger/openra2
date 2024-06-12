/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2009 Andrey Nazarov

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
==============================================================================

PACKET FILTERING


You can add or remove addresses from the filter list with:

addip <ip>
removeip <ip>

The ip address is specified in CIDR notation and supports both IPv4 and IPv6.
Leaving the bits off will result in an assumption of a /32 for IPv4 and a /128
for IPv6.
Examples:
  - addip 192.0.2.0/24
  - addip 192.0.2.5
  - addip 2002:db8:c0ff:ee::/64

Removeip will only remove an address if it matches. It can be the exact range
or if it's a bitwise match.

listip
Prints the current list of filters.

writeip
Dumps "addip <ip>" commands to listip.cfg so it can be execed at a later date.
The filter lists are not saved and restored by default, because I believe
it would cause too much confusion. It will only dump permanent filters added
from the console.

filterban <0 or 1>

If 1 (the default), then ip addresses matching the current list will be prohibited
from entering the game.  This is the default setting.

If 0, then only addresses matching the list will be allowed.  This lets you easily
set up a private game, or a game that only allows players from your local network.


==============================================================================
*/

typedef struct {
    list_t      list;
    ipaction_t  action;
    unsigned    mask, compare;
    time_t      added;
    unsigned    duration;
    netadr_t    addr;
    netadr_t    added_by;
} ipfilter_t;

#define MAX_IPFILTERS   1024
#define DEF_DURATION    (1 * 3600)
#define MAX_DURATION    (12 * 3600)

#define FOR_EACH_IPFILTER(f) \
    LIST_FOR_EACH(ipfilter_t, f, &ipfilters, list)

#define FOR_EACH_IPFILTER_SAFE(f, n) \
    LIST_FOR_EACH_SAFE(ipfilter_t, f, n, &ipfilters, list)

static LIST_DECL(ipfilters);
static int      numipfilters;

/**
 * Remove a particular entry from the filters list
 */
static void remove_filter(ipfilter_t *ip) {
    List_Remove(&ip->list);
    gi.TagFree(ip);
    numipfilters--;
}

/**
 * Add a new filter to the list.
 *
 * A duration of 0 means permanent, can only be done from the console. The ent arg is the
 * edict that added the entry to the list.
 */
static void add_filter(ipaction_t action, netadr_t addr, unsigned duration, edict_t *ent) {
    ipfilter_t *ip;

    ip = G_Malloc(sizeof(*ip));
    ip->action = action;
    ip->added = time(NULL);
    ip->duration = duration;
    ip->addr = addr;
    if (ent) {
        ip->added_by = ent->client->pers.addr;
    }
    List_Append(&ipfilters, &ip->list);
    numipfilters++;
}

/**
 * Check the IP address stored in s against all entries in the list.
 */
ipaction_t G_CheckFilters(char *s) {
    time_t      now;
    ipfilter_t  *ip, *next;
    netadr_t    addr;

    addr = net_parseIP(s);
    now = time(NULL);
    FOR_EACH_IPFILTER_SAFE(ip, next) {
        if (ip->duration && now - ip->added > ip->duration) {
            remove_filter(ip);
            continue;
        }
        if (net_contains(&ip->addr, &addr)) {
            return ip->action;
        }
    }
    return IPA_NONE;
}

/**
 * Figure out how long the ipfilter should last from the command line.
 *
 * Possible units:
 *   M = minutes
 *   H = hours
 *   D = days
 * No units defaults to minutes, case insensitive.
 */
static unsigned parse_duration(const char *s) {
    unsigned sec;
    char *p;

    sec = strtoul(s, &p, 10);
    if (*p == 0 || *p == 'm' || *p == 'M') {
        sec *= 60; // minutes are default
    } else if (*p == 'h' || *p == 'H') {
        sec *= 60 * 60;
    } else if (*p == 'd' || *p == 'D') {
        sec *= 60 * 60 * 24;
    }
    return sec;
}

/**
 * Figure out the filter type from the command line.
 *
 * Possible types:
 *   ban  = don't let the player connect
 *   mute = allow player to connect and play, but not talk
 *
 * Case insensitive
 */
static ipaction_t parse_action(const char *s) {
    if (!Q_stricmp(s, "ban")) {
        return IPA_BAN;
    }
    if (!Q_stricmp(s, "mute")) {
        return IPA_MUTE;
    }
    return IPA_NONE;
}

/**
 * Command to add a new ipfilter
 */
void G_AddIP_f(edict_t *ent) {
    unsigned duration;
    ipaction_t action;
    int start, argc;
    char *s;
    netadr_t addr;

    start = ent ? 0 : 1;
    argc = gi.argc() - start;

    if (argc < 2) {
        gi.cprintf(ent, PRINT_HIGH, "Usage: %s <ip-mask> [duration] [action]\n", gi.argv(start));
        return;
    }
    if (numipfilters == MAX_IPFILTERS) {
        gi.cprintf(ent, PRINT_HIGH, "IP filter list is full\n");
        return;
    }

    s = gi.argv(start + 1);
    addr = net_parseIPAddressMask(s);

    duration = ent ? DEF_DURATION : 0;
    action = IPA_BAN;
    if (argc > 2) {
        s = gi.argv(start + 2);
        duration = parse_duration(s);
        if (ent) {
            if (!duration) {
                gi.cprintf(ent, PRINT_HIGH, "You may not add permanent bans.\n");
                return;
            }
            if (duration > MAX_DURATION) {
                duration = MAX_DURATION;
            }
        }
        if (argc > 3) {
            s = gi.argv(start + 3);
            action = parse_action(s);
            if (action == IPA_NONE) {
                gi.cprintf(ent, PRINT_HIGH, "Bad action specifier: %s\n", s);
                return;
            }
        }
    }
    add_filter(action, addr, duration, ent);
}

/**
 * One player banning another
 */
void G_BanEdict(edict_t *victim, edict_t *initiator) {
    if (numipfilters == MAX_IPFILTERS) {
        gi.cprintf(initiator, PRINT_HIGH, "IP filter list is full\n");
        return;
    }
    add_filter(IPA_BAN, victim->client->pers.addr, DEF_DURATION, initiator);
}

/**
 * Command to remove an ip filter
 */
void G_RemoveIP_f(edict_t *ent) {
    char        *s;
    ipfilter_t  *ip;
    int         start, argc;
    netadr_t    addr;

    start = ent ? 0 : 1;
    argc = gi.argc() - start;

    if (argc < 2) {
        gi.cprintf(ent, PRINT_HIGH, "Usage: %s <ip-mask>\n", gi.argv(start));
        return;
    }

    s = gi.argv(start + 1);
    addr = net_parseIPAddressMask(s);

    FOR_EACH_IPFILTER(ip) {
        if (net_contains(&ip->addr, &addr)) {
            if (ent && !ip->duration) {
                gi.cprintf(ent, PRINT_HIGH, "You may not remove permanent bans.\n");
                return;
            }
            remove_filter(ip);
            gi.cprintf(ent, PRINT_HIGH, "%s Removed.\n", IPMASK(&ip->addr));
            return;
        }
    }
    gi.cprintf(ent, PRINT_HIGH, "Didn't find %s.\n", s);
}

/**
 * Convert seconds to a timespec
 */
static size_t Com_FormatTime(char *buffer, size_t size, time_t t) {
    int     sec, min, hour, day;

    sec = (int)t;
    min = sec / 60; sec %= 60;
    hour = min / 60; min %= 60;
    day = hour / 24; hour %= 24;

    if (day) {
        return Q_scnprintf(buffer, size, "%d+%d:%02d.%02d", day, hour, min, sec);
    }
    if (hour) {
        return Q_scnprintf(buffer, size, "%d:%02d.%02d", hour, min, sec);
    }
    return Q_scnprintf(buffer, size, "%02d.%02d", min, sec);
}

/**
 * List all filter entries
 */
void G_ListIP_f(edict_t *ent) {
    ipfilter_t *ip, *next;
    char expires[32];
    time_t now, diff;

    if (LIST_EMPTY(&ipfilters)) {
        gi.cprintf(ent, PRINT_HIGH, "Filter list is empty.\n");
        return;
    }
    now = time(NULL);
    gi.cprintf(ent, PRINT_HIGH,
               "expires in action address\n"
               "---------- ------ ------------------------------\n");
    FOR_EACH_IPFILTER_SAFE(ip, next) {
        if (ip->duration) {
            diff = now - ip->added;
            if (diff > ip->duration) {
                remove_filter(ip);
                continue;
            }
            Com_FormatTime(expires, sizeof(expires), ip->duration - diff);
        } else {
            strcpy(expires, "permanent");
        }
        gi.cprintf(ent, PRINT_HIGH, "%10s %6s %s\n",
                   expires, ip->action == IPA_MUTE ? "mute" : "ban", IPMASK(&ip->addr));
    }
}

/**
 * Write the ipfilters to a file for persistence.
 *
 * Only permanent entries are written.
 */
void G_WriteIP_f(void) {
    FILE    *f;
    char    name[MAX_OSPATH];
    size_t  len;
    ipfilter_t  *ip;

    if (!game.dir[0]) {
        return;
    }
    len = Q_snprintf(name, sizeof(name), "%s/listip.cfg", game.dir);
    if (len >= sizeof(name)) {
        return;
    }
    f = fopen(name, "wb");
    if (!f) {
        gi.cprintf(NULL, PRINT_HIGH, "Couldn't open %s\n", name);
        return;
    }
    gi.cprintf(NULL, PRINT_HIGH, "Writing %s.\n", name);
    FOR_EACH_IPFILTER(ip) {
        if (!ip->duration) {
            fprintf(f, "sv addip %s\n", IPMASK(&ip->addr));
        }
    }
    fclose(f);
}

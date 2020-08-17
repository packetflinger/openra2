# OpenRA2
Joe Reid `<claire@packetflinger.com>`

An open source remake of the Rocket Arena mod for Quake 2

### Rocket Arena?
Rocket Arena 2 is a team-based mod for Quake 2. Games are arranged into rounds 
where each player spawns with all allowed items/weapons and they battle until 
everyone from one team is dead.

### Why?
Yes, there is an existing RA2 mod that is available, but it dates back to the 
20th century and the source code is not available (or at least I'm unable to
locate it). There also don't seem to be any 64 bit binaries.

The original RA2 mod lacked many desireable features (such as chase cams) and 
this mod is aimed at fixing that. The primary score unit is damage dealt rather
than frags. While The Fast and the Furious is right, "it doesn't matter if you 
win by an inch or a mile", tracking damage rather than frags gives the loser a
little more credit.
 
### Features
* Up to 9 independently operating arenas per map
* Compatible with original RA2 maps
* Compatible with with regular maps (as a single arena)
* 2-5 teams per arena (config & voteable)
* Up to 10 players per team allowed
* Up to 21 rounds per match (config & voteable)
* Arena specific health/armor/weapon/ammo quantities (config & voteable)
* Chase cam
* Team-level chat, arena-level chat, server-level chat
* Score by damage dealt


Client commands
---------------

**accuracy**  
    See your current accuracy statistics
	
**admin**  
    Enter admin mode, elevating your privileges
	
**arena** *<arena number>*  
    Join a specific arena. If the argument is missing a list of all arenas will be displayed
	
**commands**  
    Show client commands list
	
**highscores**  
    Show highscores for your arena
	
**id**  
    Toggle player IDs
	
**join** *<teamname>*  
    Join a team
	
**matchinfo**  
    Show arena settings
	
**menu**  
    Show the GUI game menu

**obs**  
    Alias for **observer**
	
**observe**  
    Part your current team and enter spectator mode
	
**oldscore**  
    Show the scores from the last match

**players**  
    List all players connected to the server	

**ready**  
    Toggles your ready status. Once all team players are ready the match will start.
	
**settings**  
    Alias for **matchinfo**

**stats**  
    Alias for **accuracy**

**team**  
	Alias for **join**
	
**vote** *<xxx|yes|no>*  
    Call a vote or cast your vote

	
	
Admin commands
--------------

Administrators are granted access to a number of privileged client
commands.

**acommands**  
    Show administrator commands list.
	
**admin [password]**  
    Toggle administrator status.

**ban <ip-mask> [duration] [action]**  
    Add IP address specified by _ip-mask_ into the ban list.  Optional
    _duration_ parameter specifies how long this address should stay in the
    list. Default duration is 1 hour.  Maximum duration is 12 hours. Default
    units for specifying _duration_ are minutes. Add ‘h’ or ‘H’ suffix to
    specify hours. Add ‘d’ or ‘D’ suffix to specify days. Optional _action_
    parameter specifies ban type. It can be _ban_ (prevent player from
    connecting) or _mute_ (allow player to connect and enter the game, but
    disallow chat during the match). Default action is _ban_.
	
**bans**  
    Show the current ban list.

**kick <player>**  
    Kick _player_ from the server.

**kickban**  
    Kick _player_ from the server and ban his IP address for 1 hour.
	
**mute <player>**  
    Disallow _player_ to talk during the match.

**muteall**  
    Globally disable chat during the match.

**unban <ip-mask>**  
    Remove IP address specified by _ip-mask_ from the ban list. Permanent bans
    added by server operator can't be removed.
	
**unmute <player>**  
    Allow _player_ to talk during the match.
	
**unmuteall**  
    Globally enable chat during the match.


Map Config (optional)
--------------------

Each map can have it's own config file to override the default settings. These files
need to be named [mapname].cfg and saved in the *openra2/mapcfg* folder. Each file 
can contain a block (in curly braces) for each arena in the map. For example:

```
{
    arena 1
    weapons +all -bfg
    rounds 5
    damage -all +self +team
    health 115
    armor 12
    teams 2
}
```

This tells the server for arena 1 to allow all weapons but no bfg, to run 5 rounds per match, 
to do self damage and team damage, start each player with 115 health and 12 red armor. This 
also limits the arena to 2 teams.

Another example:
```
{
    arena 7
    teams 3
    weapons -all +ssg:20 +cg:inf
    timelimit 120
}
```
This sets arena 7 to have 3 teams with all players being given only a super shotgun with 20
shells, a chaingun with infinite bullets and sets the length for all rounds to 2 minutes. The default (set via CVAR) amount of health
and armor are applied to each player. 

Possible options:

**arena** <integer>

The arena number this config block applies to

**teams** <integer>

The initial number of teams

**damage** <dmgstring>

What damage can be done to you

**weapons** <weapstring>

What weapons and ammo are provided by default. weapstring is a space-separated list, each
item prefixed with a + to include or a - to exclude. Ammo quantities can be suffixed with a colon. Options: **all**, **bfg**, **hb**, **rg**, **rl**, **gl**, **cg**, **mg**, **ssg**, **sg**.

Examples: 

`-all +rl +rg:1` (Only rocket launcher and railgun (with a single slug))

`+all` (everything)

`+all -bfg` (all but the bfg)

`-all +cg:inf` (only a chaingun with infinite bullets)

**round** <integer>

The default number of rounds per match

**health** <integer>

How much health each player in this arena gets

**armor** <integer>

How much bodyarmor each player in this arena gets

**timelimit** <integer>

Number of seconds the rounds should last




Server CVARS
--------------------

**g_admin_password** <string>

The password used with the `admin` command to enable privileged commands

**g_ammo_bullets** <integer>

The default quantity of bullets players will be assigned if they have a Machinegun or Chaingun. Default: 100

**g_ammo_cells** <integer>

The default quantity of cells players will be assigned if they have a Hyperblaster or BFG. Default: 50

**g_ammo_grenades** <integer>

The default quantity of grenades players will be assigned. Default: 20

**g_ammo_rockets** <integer>

The default quantity of rockets players will be assigned. Default: 20

**g_ammo_shells** <integer>

The default quantity of shells players will be assigned. Default: 14

**g_ammo_slugs** <integer>

The default quantity of slugs players will be assigned. Default: 10

**g_armor_start** <integer>

The default quantity of bodyarmor players will start with. Default: 100

**g_bugs** <0/1/2>

Fix gameplay bugs. Default: 0

**g_damage_flags** <bitmask>

Bitmask for damage immunities, add them up. Default: 0 (everything hurts)

| Value | Meaning | Description |
| ----: | :------ | :---------- |
| 0 | Nothing | Everything hurts |
| 1 | Self health | Your explosions will NOT take your own health |
| 2 | Self armor | Your explosions will NOT take your own armor |
| 4 | Team | Your shots will NOT hurt your team mates health |
| 8 | Team Armor | Your shots will NOT hurt your team mates armor |
| 16 | Fall | Falling from high distances will NOT hurt |
| 31 | All | None of the above will hurt |

**g_default_arena** <0/1/2>

When a player connects to the server, they're immediately added to an arena, this setting controls which. Default: 1
| Value | Meaning | Description |
| ----: | :------ | :---------- |
| 0 | First | Always join arena 1 |
| 1 | Popular | Join which ever arena has the most players |
| 2 | Random | Join a random arena |

**g_demo** <0/1>

Force players to record demos of their rounds/matches. Default 0 - don't force

**g_drop_allowed** <0/1>

Allow players to drop weapons/ammo. Default 1 - allowed

**g_frag_drop** <0/1>

Players toss their weapon when killed. Default 1 - enabled

**g_health_start** <integer>

The quantity of health each player will be assigned. Default: 100

**g_idle_time** <integer>

Number of seconds to consider a team player idle and remove them from the team. Default: 0 (disabled)

**g_intermission_time** <integer>

The length in seconds match intermission should last. Default: 10

**g_round_countdown** <integer>

The number of seconds for a round countdown. Default: 12

**g_round_end_time** <integer>

The number of seconds the round intermission screen will be up before starting the next round. Default: 5

**g_round_limit** <integer>

The default number of rounds per match. Default: 7

**g_round_timelimit** <integer>

The default number of seconds each round can last. Default: 180

**g_screenshot** <0/1>

Force all players to take a screenshot of the match intermission screen. Default 0 (no)

**g_select_empty** <0/1>

All clients to "use" a weapon without ammo for it. Default 0 (no)

**g_skin_lock** <0/1>

Lock team skins so they can't be changed. Default 0 (no, allow change)

**g_team1_name** <string>

The name of team1. Default `Red`

**g_team1_skin** <string>

The skin used for team1. Default `female/r2red`

**g_team2_name** <string>

The name of team2. Default `Blue`

**g_team2_skin** <string>

The skin used for team2. Default `male/r2blue`

**g_team3_name** <string>

The name of team3. Default `Green`

**g_team3_skin** <string>

The skin used for team3. Default `female/r2dgre`

**g_team4_name** <string>

The name of team4. Default `Yellow`

**g_team4_skin** <string>

The skin used for team4. Default `male/r2yell`

**g_team5_name** <string>

The name of team5. Default `Aqua`

**g_team5_skin** <string>

The skin used for team5. Default `female/r2aqua`

**g_team_balance** <0/1>

Require teams to be balanced with equal players. Default 0 (allow unbalanced)

**g_team_count** <2-5>

The default number of teams per arena. Default 2

**g_team_reset** <0/1>

Remove all players from teams at the end of the match. Default 0 (teams remain)

**g_teamspec_name** <string>

The name of the spectator "team". Default `Spectators`

**g_teleporter_nofreeze** <0/1>

Quake 2-style vs Quake 3-style teleporters. Default 0 (q2 style)

**g_timein_time** <integer>

Time in seconds for time-in countdown. Default 11

**g_timeout_time** <integer>

Time in seconds for time-outs. Default 180

**g_vote_threshold** <integer>

Percentage of players required to pass a vote. Default 51

**g_vote_time** <integer>

Time in seconds for how long a vote will last. Default 30

**g_weapon_flags** <bitmask>

Bitmask for what weapons are allowed, add them up. Default 1023 (all except for BFG)

| Index | Weapon |
| ----: | :----- |
| 2 | Shotgun |
| 4 | Super Shotgun |
| 8 | Machinegun |
| 16 | Chaingun |
| 32 | Grenades |
| 64 | Grenade Launcher |
| 128 | Hyperblaster |
| 256 | Rocket Launcher |
| 512 | Railgun |
| 1024 | BFG |


Very Simple Example Server Config
---------------------
```
set hostname         "My Awesome OpenRA2 Server"
set net_port         "27910" // use "port" for r1q2, "net_port" for q2pro
set rcon_password    "<insert rcon pwd here>"
set g_admin_password "<insert admin pwd here>"
set g_team_count     "2"
set g_health_start   "110"
set g_armor_start    "30"
```

Example Map Config
---------------------

For ra2map27, this file would be named `mapcfg/ra2map27.cfg`
```
{
    arena 1
    weapons +all -bfg
    rounds 3
    damage -all +self +team
    health 115
    armor 12
    teams 2
    timelimit 120
}

{
    arena 2
    weapons -all +ssg:12 +rl:10
    rounds 5
    damage 8
}

{
    arena 3
    weapons +all
    rounds 5
}

{
    arena 4
    weapons -all +rg:inf
    rounds 5
}

{
    arena 5
    teams 5
}
```

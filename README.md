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




Server configuration (Under Construction)
--------------------





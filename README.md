# OpenRA2
Joe Reid <claire@packetflinger.com>

An open source remake of the Rocket Arena mod for Quake 2

### Rocket Arena?
Rocket Arena 2 is a team-based mod for Quake 2. Games are arranged into rounds 
where each player spawns with all allowed items/weapons and they battle until 
everyone from one team is dead.

### Why?
Yes, there is an existing RA2 mod that is available, but it dates back to the 
20th century and the source code is not available (or at least I'm unable to
locate it). There also don't seem to be any 64 bit binaries. 
 
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


	
Server configuration
--------------------

Under Construction
<!---
g_idle_time::
    Time, in seconds, after which inactive players are automatically put into
    spectator mode. Default value is 0 (don't remove inactive players).

g_maps_random::
    Specifies whether map list is traversed in random on sequental order.
    Default value is 2.
       - 0 - sequental order
       - 1 - random order
       - 2 - random order, never allows the same map to be picked twice in a row

g_defaults_file::
    If this variable is not empty and there are some settings modified by
    voting, server will execute the specified config file after 5 minutes pass
    without any active players. Config file should reset all votable variables
    to their default values. Default value is empty.


g_bugs::
    Specifies whether some known Quake 2 gameplay bugs are enabled or not.
    Default value is 0.
       - 0 - all bugs are fixed
       - 1 - ‘serious’ bugs are fixed
       - 2 - original Quake 2 behaviour

g_teleporter_nofreeze::
    Enables ‘no freeze’ (aka ‘Q3’) teleporter behaviour. Default value is 0
    (disabled).

g_vote_mask::
    Specifies what proposals are available for voting. This variable is a
    bitmask.  Default value is 0.
       - 1 - change time limit
       - 2 - change frag limit
       - 4 - change item bans
       - 8 - kick a player
       - 16 - mute a player
       - 32 - change current map
       - 64 - toggle weapon stay
       - 128 - toggle respawn protection (between 0 and 1.5 sec)
       - 256 - change teleporter mode

g_vote_time::
    Time, in seconds, after which undecided vote times out. Default value is
    60.

g_vote_treshold::
    Vote passes or fails when percentage of players who voted either ‘yes’ or
    ‘no’ becomes greater than this value. Default value is 50.

g_vote_limit::
    Maximum number of votes each player can initiate. Default value is 3.  0
    disables this limit.

g_vote_flags::
    Specifies misc voting parameters. This variable is a bitmask. Default value
    is 11.
        - 1 - each player's decision is globally announced as they vote
        - 2 - current vote status is visible in the left corner of the screen
        - 4 - spectators are also allowed to vote
        - 8 - players are allowed to change their votes

g_intermission_time::
    Time, in seconds, for the final scoreboard and high scores to be visible
    before automatically changing to the next map. Default value is 10.

g_admin_password::
    If not empty, clients can execute ‘admin <password>’ command to become
    server admins. Right now this gives them a decider voice in votes, ability
    to see IP addresses in the output of ‘playerlist’ command and grants access
    to a number of privileged commands (listed in ‘acommands’ command output).
    Default value is empty (admin feature disabled).

g_mute_chat::
    Allows one to globally disallow chat during the match (chat is still
    allowed during the intermission). Default value is 0.
       - 0 - chat is enabled for everyone
       - 1 - player chat is disabled, spectators are forced to use ‘say_team’
       - 2 - chat is disabled for everyone

flood_msgs::
    Number of the last chat message considered by flood protection algorithm.
    Default value is 4. Specify 0 to disable chat flood protection.

flood_persecond::
    Minimum time, in seconds, that has to pass since the last chat message
    before flood protection is triggered. Default value is 4.

flood_waitdelay::
    Time, in seconds, for player chat to be disabled once flood protection is
    triggered. Default value is 10.

flood_waves::
    Number of the last wave command considered by flood protection algorithm.
    Default value is 4. Specify 0 to disable wave flood protection.

flood_perwave::
    Minimum time, in seconds, that has to pass since the last wave command
    before flood protection is triggered. Default value is 30.

flood_wavedelay::
    Time, in seconds, for wave commands to be disabled once flood protection is
    triggered. Default value is 60.

flood_infos::
    Number of the last name or skin change considered by flood protection
    algorithm.  Default value is 4. Specify 0 to disable userinfo flood
    protection.

flood_perinfo::
    Minimum time, in seconds, that has to pass since the last name or skin
    change before flood protection is triggered. Default value is 30.

flood_infodelay::
    Time, in seconds, for name or skin changes to be disabled once flood
protection is triggered. Default value is 60.
-->

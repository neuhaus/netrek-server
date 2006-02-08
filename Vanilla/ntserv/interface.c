/*
 * interface.c
 *
 * This file will include all the interfaces between the input routines
 *  and the daemon.  They should be useful for writing robots and the
 *  like 
 */
#include "copyright.h"

#include <stdio.h>
#include <math.h>
#include <signal.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "proto.h"

/* file scope prototypes */
static void sendwarn(char *string, int atwar, int team);

void set_speed(int speed)
{
    if (speed > me->p_ship.s_maxspeed) {
	me->p_desspeed = me->p_ship.s_maxspeed;
    } else if (speed < 0) {
	speed=0;
    }
    me->p_desspeed = speed;
    if (me->p_flags & PFDOCK) {
	players[me->p_docked].p_docked--;
	players[me->p_docked].p_port[me->p_port[0]] = VACANT;
    }
    me->p_flags &= ~(PFREPAIR | PFBOMB | PFORBIT | PFDOCK | PFBEAMUP | PFBEAMDOWN);
}

void set_course(u_char dir)
{
    me->p_desdir = dir;
    if (me->p_flags & PFDOCK) {
	players[me->p_docked].p_docked--;
	players[me->p_docked].p_port[me->p_port[0]] = VACANT;
    }
    me->p_flags &= ~(PFBOMB | PFORBIT | PFDOCK | PFBEAMUP | PFBEAMDOWN);
}

void shield_up(void)
{
    me->p_flags |= PFSHIELD;
    me->p_flags &= ~(PFBOMB | PFREPAIR | PFBEAMUP | PFBEAMDOWN);
}

void shield_down(void)
{
    me->p_flags &= ~PFSHIELD;
}

void shield_tog(void)
{
    me->p_flags ^= PFSHIELD;
    me->p_flags &= ~(PFBOMB | PFREPAIR | PFBEAMUP | PFBEAMDOWN);
}

void bomb_planet(void)
{
    static int bombsOutOfTmode = 0; /* confirm bomb out of t-mode 4/6/92 TC */
    int owner;			/* temporary variable 4/6/92 TC */

    if (!(me->p_flags & PFORBIT)) {
        new_warning(39,"Must be orbiting to bomb");
	return;
    }

    /* no bombing of own armies, friendlies 4/6/92 TC */

    owner = planets[me->p_planet].pl_owner;

    if (me->p_team == owner) {
        new_warning(40,"Can't bomb your own armies.  Have you been reading Catch-22 again?");
	return;
    }
    if (!(me->p_war & owner)) {
        new_warning(41,"Must declare war first (no Pearl Harbor syndrome allowed here).");
	return;
    }

    if(restrict_bomb
#ifdef PRETSERVER
            /* if this is the pre-T entertainment we don't require confirmation */
            && !bot_in_game 
#endif
        ) {
        if (!status->tourn){
            new_warning(UNDEF,"You may not bomb out of T-mode.");
          return;
        }
    }

    if ((!status->tourn) && (bombsOutOfTmode == 0)
#ifdef PRETSERVER
            /* if this is the pre-T entertainment we don't require confirmation */
            && !bot_in_game 
#endif
        ) {
        new_warning(42,"Bomb out of T-mode?  Please verify your order to bomb.");
        bombsOutOfTmode++;
        return;
    }

#ifdef PRETSERVER
    if(bot_in_game && realNumShips(owner) == 0) {
        new_warning(UNDEF,"You may not bomb 3rd and 4th space planets.");
        return;
    }
#endif

    if(no_unwarring_bombing) {
/* Added ability to take back your own planets from 3rd team 11-15-93 ATH */
        if ((status->tourn && realNumShips(owner) < tournplayers) 
          && !(me->p_team & planets[me->p_planet].pl_flags)) {
            new_warning(UNDEF,"You may not bomb 3rd and 4th space planets.");
            return;
         }
    }

    if(! restrict_bomb
#ifdef PRETSERVER
            /* if this is the pre-T entertainment we don't require confirmation */
            && !bot_in_game 
#endif
        )
    {
        if ((!status->tourn) && (bombsOutOfTmode == 1)) {
            new_warning(43,"Hoser!");
            bombsOutOfTmode++;
        }
    }

    if (status->tourn) bombsOutOfTmode = 0;

    me->p_flags |= PFBOMB;
    me->p_flags &= ~(PFSHIELD | PFREPAIR | PFBEAMUP | PFBEAMDOWN);
}

void beam_up(void)
{
    if (!(me->p_flags & (PFORBIT | PFDOCK))) {
        new_warning(44,"Must be orbiting or docked to beam up.");
	return;
    }
    if (me->p_flags & PFORBIT) {
	if (me->p_team != planets[me->p_planet].pl_owner) {
            new_warning(45,"Those aren't our men.");
	    return;
	}
    } else if (me->p_flags & PFDOCK) {
	if (me->p_team != players[me->p_docked].p_team) {
            new_warning(46,"Comm Officer: We're not authorized to beam foriegn troops on board!");
	    return;
	}
    }
    me->p_flags |= PFBEAMUP;
    me->p_flags &= ~(PFSHIELD | PFREPAIR | PFBOMB | PFBEAMDOWN);
}

void beam_down(void)
{
    int owner;

    if (!(me->p_flags & (PFORBIT | PFDOCK))) {
        new_warning(47, "Must be orbiting or docked to beam down.");
        return;
    }

#ifdef PRETSERVER
    if(pre_t_mode && me->p_flags & PFORBIT) {
        owner = planets[me->p_planet].pl_owner;
        if(bot_in_game && realNumShips(owner) == 0 && owner != NOBODY) {
            new_warning(UNDEF,"You may not drop on 3rd and 4th space planets. Sorry Bill.");
            return;
        }
    }
#endif

    if (me->p_flags & PFDOCK) {
        if (me->p_team != players[me->p_docked].p_team) {
            new_warning(48,"Comm Officer: Starbase refuses permission to beam our troops over.");
	    return;
        }
    }
    me->p_flags |= PFBEAMDOWN;
    me->p_flags &= ~(PFSHIELD | PFREPAIR | PFBOMB | PFBEAMUP);
}

void repair(void)
{
#ifndef NEW_ETEMP
    if ((me->p_flags & PFENG) && (me->p_speed != 0)) {
	new_warning(UNDEF,"Sorry, can't repair with melted engines while moving.");
    } else 
#endif
    {
	me->p_desspeed = 0;
	me->p_flags |= PFREPAIR;
	me->p_flags &= ~(PFSHIELD | PFBOMB | PFBEAMUP | PFBEAMDOWN | PFPLOCK | PFPLLOCK);
    }
}

void repair_off(void)
{
    me->p_flags &= ~PFREPAIR;
}

void repeat_message(void)
{
    if (++lastm == MAXMESSAGE) ;
	lastm = 0;
}

void cloak(void)
{
    me->p_flags ^= PFCLOAK;
    me->p_flags &= ~(PFTRACT | PFPRESS);
}

void cloak_on(void)
{
    me->p_flags |= PFCLOAK;
    me->p_flags &= ~(PFTRACT | PFPRESS);
}

void cloak_off(void)
{
    me->p_flags &= ~PFCLOAK;
}

void lock_planet(int planet)
{
    if (planet<0 || planet>=MAXPLANETS) return;

    me->p_flags |= PFPLLOCK;
    me->p_flags &= ~(PFPLOCK|PFORBIT|PFBEAMUP|PFBEAMDOWN|PFBOMB);
    me->p_planet = planet;
    if(send_short) {
                swarning(LOCKPLANET_TEXT, (u_char) planet, 0);
    }
    else {
    new_warning(UNDEF,"Locking onto %s", planets[planet].pl_name);
     }
}

void lock_player(int player)
{
    if (player<0 || player>=MAXPLAYER) return;
    if (players[player].p_status != PALIVE) return;
    if (players[player].p_flags & PFCLOAK && !Observer) return;

    me->p_flags |= PFPLOCK;
    me->p_flags &= ~(PFPLLOCK|PFORBIT|PFBEAMUP|PFBEAMDOWN|PFBOMB);
    me->p_playerl = player;

    /* notify player docking perm status of own SB when locking 7/19/92 TC */

    if ((players[player].p_team == me->p_team) &&
	(players[player].p_ship.s_type == STARBASE) &&
	(me->p_candock)) 
        {
        if(send_short) {
                swarning(SBLOCKMYTEAM,player,0);
                return;
        }
        else
	new_warning(UNDEF, "Locking onto %s (%c%c) (docking is %s)",
		players[player].p_name,
		teamlet[players[player].p_team],
		shipnos[players[player].p_no],
		(players[player].p_flags & PFDOCKOK) ? "enabled" : "disabled");
        }
    else
        {
        if(send_short) {
                swarning(SBLOCKSTRANGER,player,0);
                return;
        }
        else
	new_warning(UNDEF, "Locking onto %s (%c%c)",
		players[player].p_name,
		teamlet[players[player].p_team],
		shipnos[players[player].p_no]);
        }
}

void tractor_player(int player)
{
    struct player *victim;

    if (weaponsallowed[WP_TRACTOR]==0) {
	return;
    }
    if ((player < 0) || (player > MAXPLAYER)) {	/* out of bounds = cancel */
	me->p_flags &= ~(PFTRACT | PFPRESS);
	return;
    }
    if (me->p_flags & PFCLOAK) {
	return;
    }
    if (player==me->p_no) return;
    victim= &players[player];
    if (victim->p_flags & PFCLOAK) return;
    if (hypot((double) me->p_x-victim->p_x,
	    (double) me->p_y-victim->p_y) < 
	    ((double) TRACTDIST) * me->p_ship.s_tractrng) {
	if (victim->p_flags & PFDOCK) {
	    players[victim->p_docked].p_port[victim->p_port[0]] = VACANT;
	    players[victim->p_docked].p_docked--;
	}
	if (me->p_flags & PFDOCK) {
	    players[me->p_docked].p_docked--;
	    players[me->p_docked].p_port[me->p_port[0]] = VACANT;
	}
	victim->p_flags &= ~(PFORBIT | PFDOCK);
	me->p_flags &= ~(PFORBIT | PFDOCK);
	me->p_tractor = player;
	me->p_flags |= PFTRACT;
    } else {			/* out of range */
    }
}

void pressor_player(int player)
{
    int target;
    struct player *victim;

    if (weaponsallowed[WP_TRACTOR]==0) {
        new_warning(0,"Tractor beams haven't been invented yet.");
	return;
    }

    target=player;

    if ((target < 0) || (target > MAXPLAYER)) {	/* out of bounds = cancel */
        me->p_flags &= ~(PFTRACT | PFPRESS);
        return;
    }
    if (me->p_flags & PFPRESS) return; /* bug fix: moved down 5/1/92 TC */

    if (me->p_flags & PFCLOAK) {
        new_warning(1,"Weapons's Officer:  Cannot tractor while cloaked, sir!");
	return;
    }
    
    victim= &players[target];
    if (victim->p_flags & PFCLOAK) return;
    
    if (hypot((double) me->p_x-victim->p_x,
	      (double) me->p_y-victim->p_y) < 
	((double) TRACTDIST) * me->p_ship.s_tractrng) {
	if (victim->p_flags & PFDOCK) {
	    players[victim->p_docked].p_port[victim->p_port[0]] = VACANT;
	    players[victim->p_docked].p_docked--;
	}
	if (me->p_flags & PFDOCK) {
	    players[me->p_docked].p_docked--;
	    players[me->p_docked].p_port[me->p_port[0]] = VACANT;
	}
	victim->p_flags &= ~(PFORBIT | PFDOCK);
	me->p_flags &= ~(PFORBIT | PFDOCK);
	me->p_tractor = target;
	me->p_flags |= (PFTRACT | PFPRESS);
    } else {
        new_warning(2,"Weapon's Officer:  Vessel is out of range of our tractor beam.");
    }
    
}

void declare_war(int mask)
{
    int changes;
    int i;

    /* indi are forced to be at peace with everyone */

    if( !(me->p_flags & PFROBOT) && (me->p_team == NOBODY) ) {
      me->p_war = NOBODY;
      me->p_hostile = NOBODY;
      me->p_swar = NOBODY;
      return;
    }

    mask &= ALLTEAM;
    mask &= ~me->p_team;
    mask |= me->p_swar;
    changes=mask ^ me->p_hostile;
    if (changes==0) return;

    if (changes & FED) {
	sendwarn("Federation", mask & FED, FED);
    }
    if (changes & ROM) {
	sendwarn("Romulans", mask & ROM, ROM);
    }
    if (changes & KLI) {
	sendwarn("Klingons", mask & KLI, KLI);
    }
    if (changes & ORI) {
	sendwarn("Orions", mask & ORI, ORI);
    }
    if (me->p_flags & PFDOCK) {
	if (players[me->p_docked].p_team & mask) {
	    players[me->p_docked].p_port[me->p_port[0]] = VACANT;
	    players[me->p_docked].p_docked--;
	    me->p_flags &= ~PFDOCK;
	}
    } else if (me->p_ship.s_type == STARBASE) {
	if (me->p_docked > 0) {
	    for(i=0; i<NUMPORTS; i++) {
		if (me->p_port[i] == VACANT)   /* isae -- Ted's fix */
                  continue;
		if (mask & players[me->p_port[i]].p_team) {
		    players[me->p_port[i]].p_flags &= ~PFDOCK;
		    me->p_docked--;
		    me->p_port[i] = VACANT;
		}
	    }
	}
    }

    if (mask & ~me->p_hostile) {
	me->p_flags |= PFWAR;
	delay = me->p_updates + 100;
        new_warning(49,"Pausing ten seconds to re-program battle computers.");
    }
    me->p_hostile = mask;
    me->p_war = (me->p_swar | me->p_hostile);
}

static void sendwarn(char *string, int atwar, int team)
{
    char addrbuf[10];

    (void) sprintf(addrbuf, " %c%c->%-3s",
        teamlet[me->p_team],
        shipnos[me->p_no],
        teamshort[team]);

    if (atwar) {
	pmessage2(team, MTEAM, addrbuf, me->p_no,
            "%s (%c%c) declaring war on the %s",
            me->p_name,
            teamlet[me->p_team],
            shipnos[me->p_no],
            string);
    }
    else {
	pmessage2(team, MTEAM, addrbuf, me->p_no,
            "%s (%c%c) declaring peace with the %s",
            me->p_name,
            teamlet[me->p_team],
            shipnos[me->p_no],
            string);
    }
}

void do_refit(int type)
{	
    int i=0;
    struct ship_cap_spacket ShipFoo;

    if (type<0 || type>=ATT) return;
    if (me->p_flags & PFORBIT) {
	if (!(planets[me->p_planet].pl_flags & PLHOME)) {
            new_warning(50,"You must orbit your HOME planet to apply for command reassignment!");
	    return;
	} else {
	    if (!(planets[me->p_planet].pl_flags & me->p_team)) {
                new_warning(51,"You must orbit your home planet to apply for command reassignment!");
		return;
	    }
	} /* if (PLHOME) */
    } else if (me->p_flags & PFDOCK) {
	if (type == STARBASE

	) {
            new_warning(52,"Can only refit to starbase on your home planet.");
	    return;
	}
	if (players[me->p_docked].p_team != me->p_team) {
            new_warning(53,"You must dock YOUR starbase to apply for command reassignment!");
	    return;
	}
    } else {
        new_warning(54,"Must orbit home planet or dock your starbase to apply for command reassignment!");
	return;
    } 

    if ((me->p_damage> ((float)me->p_ship.s_maxdamage)*.75) || 
	    (me->p_shield < ((float)me->p_ship.s_maxshield)*.75) ||
	    (me->p_fuel < ((float)me->p_ship.s_maxfuel)*.75)) {
        new_warning(55,"Central Command refuses to accept a ship in this condition!");
	return;
    }

    if ((me->p_armies > 0)) {
        new_warning(56,"You must beam your armies down before moving to your new ship");
	return;
    }

    if (shipsallowed[type]==0) {
        new_warning(57,"That ship hasn't been designed yet.");
	return;
    }

    if (type == STARBASE) {
	if (me->p_stats.st_rank < sbrank) {
            if(send_short){
                swarning(SBRANK_TEXT,sbrank,0);
            }
            else {

	    new_warning(UNDEF,"You need a rank of %s or higher to command a starbase!", ranks[sbrank].name);
            }
	    return;
	}
    }
    if (type == STARBASE && chaos) {
	int num_bases = 0;
	for (i=0; i<MAXPLAYER; i++) 
     	    if ((me->p_no != i) && 
		(players[i].p_status == PALIVE) && 
		(players[i].p_team == me->p_team))
		if (players[i].p_ship.s_type == STARBASE) {
		  num_bases++;
		}
	if (num_bases >= max_chaos_bases) {
	  new_warning(UNDEF,"Your side already has %d starbase%s",max_chaos_bases,(max_chaos_bases>1) ? "s!":"!");
	  return;
	}
    }
    else if (type == STARBASE && !chaos) {
	for (i=0; i<MAXPLAYER; i++) 
	    if ((me->p_no != i) && 
		(players[i].p_status == PALIVE) && 
		(players[i].p_team == me->p_team))
		if (players[i].p_ship.s_type == STARBASE) {
                    new_warning(58,"Your side already has a starbase!");
		    return;
		}
    }
    if (type == STARBASE && !chaos && !topgun) {
	if (realNumShips(me->p_team) < 4) {
            if(send_short){
                swarning(TEXTE,59,0);
            }
            else
	    new_warning(UNDEF,"Your team is not capable of defending such an expensive ship");
	    return;
	}
    }
    if (type == STARBASE && !chaos && !topgun) { /* change TC 12/10/90 */
	if (teams[me->p_team].s_turns > 0) {
	  new_warning(UNDEF,"Not constructed yet. %d minutes required for completion",teams[me->p_team].s_turns);
	    return;
	}
    }
    if (type == STARBASE && numPlanets(me->p_team) < sbplanets && !topgun) {
        new_warning(61,"Your team's stuggling economy cannot support such an expenditure!");
	return;
    }

    if ((me->p_ship.s_type == STARBASE)/* && (type != STARBASE)*/) {
	/* Reset kills to 0.0 */
	me->p_kills=0;
	/* bump all docked ships */
	for (i=0; i<NUMPORTS; i++) 
	   if (me->p_port[i] != VACANT) {
		players[me->p_port[i]].p_flags &= ~PFDOCK;
		me->p_docked--;
		me->p_port[i] = VACANT;	
		me->p_flags |= PFDOCKOK;
	   }
    }	

    
    getship(&(me->p_ship), type);
    /* Notify client of new ship stats, if necessary */
#ifndef ROBOT
    if ((F_ship_cap)&&(!sent_ship[type])) {
	sent_ship[type] = 1;
	ShipFoo.type = SP_SHIP_CAP;
	ShipFoo.s_type = htons(me->p_ship.s_type);
	ShipFoo.operation = 0;
	ShipFoo.s_torpspeed = htons(me->p_ship.s_torpspeed);
	ShipFoo.s_maxfuel = htonl(me->p_ship.s_maxfuel);
	ShipFoo.s_maxspeed = htonl(me->p_ship.s_maxspeed);
	ShipFoo.s_maxshield = htonl(me->p_ship.s_maxshield);
	ShipFoo.s_maxdamage = htonl(me->p_ship.s_maxdamage);
	ShipFoo.s_maxwpntemp = htonl(me->p_ship.s_maxwpntemp);
	ShipFoo.s_maxegntemp = htonl(me->p_ship.s_maxegntemp);
	ShipFoo.s_width = htons(me->p_ship.s_width);
	ShipFoo.s_height = htons(me->p_ship.s_height);
	ShipFoo.s_maxarmies = htons(me->p_ship.s_maxarmies);
	ShipFoo.s_letter = "sdcbaog*"[me->p_ship.s_type];
	ShipFoo.s_desig1 = shiptypes[type][0];
	ShipFoo.s_desig2 = shiptypes[type][1];
	ShipFoo.s_phaserrange = htons(me->p_ship.s_phaserdamage);
	ShipFoo.s_bitmap = htons(me->p_ship.s_type);
	strcpy(ShipFoo.s_name,shipnames[me->p_ship.s_type]);
	sendClientPacket((CVOID) &ShipFoo);
    }
#endif
    if (type != STARBASE && me->p_kills < plkills) {
	me->p_ship.s_plasmacost = -1;
    }
    me->p_shield = me->p_ship.s_maxshield;
    me->p_damage = 0;
    me->p_fuel = me->p_ship.s_maxfuel;
    me->p_wtemp = 0;
    me->p_wtime = 0;
    me->p_etemp = 0;
    me->p_etime = 0;
    me->p_ship.s_type = type;
    if (type == STARBASE) {
	me->p_docked = 0;
	for (i=0; i<4; i++) me->p_port[i] = VACANT;
	me->p_flags |= PFDOCKOK;
    }

    me->p_flags &= ~(PFREFIT|PFWEP|PFENG);
    me->p_flags |= PFREFITTING;
    rdelay = me->p_updates + 50;

    new_warning(62,"You are being transported to your new vessel .... ");
}

int numPlanets(int owner)
{
    int i, num=0;
    struct planet *p;

    for (i=0, p=planets; i<MAXPLANETS; i++, p++) {
        if (p->pl_owner==owner)  {
            num++;
        }
    }
    return(num);
}

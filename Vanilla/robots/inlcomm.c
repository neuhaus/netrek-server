/* inlcomm.c
*/

/*
   INL server robot by Nick Trown & Kurt Siegl

   Many modifications done by Kevin O'Connor
*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <pwd.h>
#include <ctype.h>
#include <time.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "inldefs.h"

extern Inl_stats inl_stat;
extern Team	 inl_teams[];
extern int	 inl_start;
extern Sides	 sides[];
extern char	*inl_from;
extern Inl_countdown inl_countdown;

extern char	*addr_mess(int who, int type);
extern int	doResources();
extern void	inl_freeze();
extern int	check_winner();

int
check_player(who, captain)
     int who, captain;
{
  int c,  num = NONE;

#ifdef INLDEBUG
  ERROR(2,("	Enter check_player\n"));
#endif

  if ((players[who].w_queue != QU_HOME) && 
      (players[who].w_queue != QU_AWAY) &&
      (players[who].w_queue != QU_HOME_OBS) && 
      (players[who].w_queue != QU_AWAY_OBS) )
    {
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "You are not on a team. Send yourself 'HOME' or 'AWAY'");
      return NONE;
    }

  for (c=0; c < INLTEAM; c++)
    {
      if (players[who].p_team == inl_teams[c].side)
	{
	  num = c;
	  break;
	}
    }
  if (num == NONE)
    {
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "You need to pick a team first.");
      return NONE;
    }

  if (captain)
    {
      if (inl_teams[num].captain != who) 
	{
	  pmessage(who, MINDIV, addr_mess(who, MINDIV),
		   "You are not the captain of your team.");
	  return NONE;
	}
    }
  return num;
}



do_switchside(comm,mess)
     char *comm;
     struct message *mess;
{
  int who;
  int queue;
  int side;
  struct player *j;

#ifndef nodef
  return 0;	/* disabled until fixed */

  /* code in ntserv/queue.c sets the low_slot and high_slot of the QU_HOME and
     QU_AWAY queues such that moving slots between teams as this code does
     causes situations in which there may be, say, seven players on a team but
     with a wait queue.  cameron@stl.dec.com */

#else

#ifdef INLDEBUG
  ERROR(2,("	Enter do_switchside\n"));
#endif

  who = mess->m_from;

#ifdef nodef
  /* Should we allow changing the side during a game? [007] */
  if (!inl_stat.change) return 0;		/* can't change it anymore */
#endif

  if (inl_teams[check_player(who, 0)].captain == who)
    {
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "You can't switch while you are captain!");
      return 0;
    }

  if (strcmp(comm, "AWAY") == 0)
    {
      side = AWAY;
      queue = QU_AWAY;
    }
  else if (strcmp(comm, "HOME") == 0)
    {
      side = HOME;
      queue = QU_HOME;
    }
  else
    {
      ERROR(1,("Unknown side passed to do_switchside (%s)\n", comm));
      return 0;
    }

  if (players[who].w_queue == queue)
    {
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "You are already on the %s team.",
	       inl_teams[side].name);
      return 0;
    }
  else if (queues[queue].free_slots)
    {
      j = &players[who];
      queues[j->w_queue].free_slots++;
      j->w_queue = queue;
      queues[queue].free_slots--;
      j->p_team = inl_teams[side].side;
      sprintf(j->p_mapchars,"%c%c",
	      teamlet[j->p_team], shipnos[j->p_no]);
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "You are now on the %s team.", inl_teams[side].name);
      j->p_status = PEXPLODE;
      j->p_whydead = KPROVIDENCE;
      if (j->p_ship.s_type == STARBASE)
	j->p_explode = 2 * SBEXPVIEWS ;
      else
	j->p_explode = 10 ;
      j->p_ntorp = 0;
      j->p_nplasmatorp = 0;
      j->p_hostile = 0;
      j->p_war = (j->p_swar | j->p_hostile);
      return 1;
    }
  else
    pmessage(who, MINDIV, addr_mess(who, MINDIV),
	     "You can't switch teams now, no free slots on the other team.");
#endif /* ! nodef */
}


/* Allows the captain to agree on starting the game */

do_start(comm,mess)
     char *comm;
     struct message *mess;
{
  int who;
  int c, num = -1;
  int begin = 0;

#ifdef INLDEBUG
  ERROR(2,("	Enter do_start\n"));
#endif

  who = mess->m_from;

  if (!inl_stat.change)
    {			/* can't change it anymore */
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "You can no longer start. The game has started.");
      return 0;
    }

  if ((num = check_player(who, 1)) == NONE)
    return 0;

  if ((inl_teams[num].side_index == NOT_CHOOSEN)
      || (inl_teams[num].side_index == RELINQUISH))
    {
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "You have to pick a side first");
      return 0;
    }

  inl_teams[num].flags |= T_START;

  pmessage(0, MALL, inl_from, "%s (%s) requests the game to start.", 
	   inl_teams[num].t_name, players[who].p_mapchars);

  for (c=0; c < INLTEAM; c++)
    {
      if (inl_teams[c].flags & T_START) begin++;
    }
#ifdef INLDEBUG
  ERROR(2,("	  begin = %d\n",begin));
#endif

  if (begin != 2) return 1;

  /* #ifdef nodef */  /* why was this nodef here? */
  if ((inl_teams[HOME].time != inl_teams[AWAY].time ) ||
      (inl_teams[HOME].overtime != inl_teams[AWAY].overtime))
    {
      pmessage(0, MALL, inl_from,
	       "The Game time has not be approved by both sides!");
      for (c = 0; c < INLTEAM; c++)
	{
	  pmessage(0, MALL, inl_from,
		   "%s (%s) has %d mins regulation and %d overtime",
		   inl_teams[c].t_name, inl_teams[c].name,
		   inl_teams[c].time, inl_teams[c].overtime);
	}
      return 0;
    }
  /* #endif */

  if (inl_teams[HOME].start_armies != inl_teams[AWAY].start_armies)
    {
      pmessage(0, MALL, inl_from,
               "The initial army count has not be approved by both sides!");
      for (c = 0; c < INLTEAM; c++)
        {
          pmessage(0, MALL, inl_from,
                   "%s (%s) has set %d armies",
                   inl_teams[c].t_name, inl_teams[c].name,
                   inl_teams[c].start_armies);
        }
      return 0;
    }

  if (inl_teams[HOME].score_mode != inl_teams[AWAY].score_mode)
    {
      pmessage(0, MALL, inl_from,
               "Scoring mode has not be approved by both sides!");
      for (c = 0; c < INLTEAM; c++)
        {
          pmessage(0, MALL, inl_from,
                   "%s (%s) has set score mode to %d",
                   inl_teams[c].t_name, inl_teams[c].name,
                   inl_teams[c].score_mode);
        }
      return 0;
    }

  for (c=0; c < INLTEAM; c++)
    inl_teams[c].flags &= ~T_START;

  start_countdown();
}

do_register(comm,mess)
     char *comm;
     struct message *mess;
{
  int who;
  int c, num = -1;
  int official = 0;

  who = mess->m_from;

  if ((num = check_player(who, 1)) == NONE)
    return 0;

  inl_teams[num].flags ^= T_REGISTER;

  pmessage(0, MALL, inl_from, "%s team (%s) claims this is an %s game.",
	   inl_teams[num].t_name, players[who].p_mapchars,
	   inl_teams[num].flags & T_REGISTER ? "Official" : "Unofficial" );

  for (c=0; c < INLTEAM; c++) {
    if (inl_teams[c].flags & T_REGISTER) official++;
  }

  if (official != 2) return 1;

  pmessage(0, MALL, inl_from,
	   "Game is official.. will be automatically registered.");
}

do_gametime(comm,mess)
     char *comm;
     struct message *mess;
{
  int who;
  int time, overtime;
  int num;
  int i;

#ifdef INLDEBUG
  ERROR(2,("	Enter do_settime\n"));
#endif

  who = mess->m_from;

  if (!(inl_stat.flags & S_PREGAME))
    {
      time = inl_stat.time;
      if (inl_stat.flags & S_OVERTIME)
	time += inl_stat.overtime;

      time += inl_stat.extratime;

#ifdef nodef
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "Minutes remaining: %i	Planet tally: %i - %i - %i",
	       ((time - inl_stat.game_ticks) / PERMIN ),
	       inl_teams[HOME].planets, inl_teams[AWAY].planets,
	       (20 - inl_teams[HOME].planets - inl_teams[AWAY].planets));
#else
     {
      int seconds = ( (time - inl_stat.game_ticks) / PERSEC ) % 60;
      int minutes = ( (time - inl_stat.game_ticks) / PERMIN );

      if (inl_stat.extratime)
        pmessage(who, MINDIV, addr_mess(who, MINDIV),
                 "Extra Time remaining: %d:%02.2d", minutes, seconds);
      else
        pmessage(who, MINDIV, addr_mess(who, MINDIV),
                 "Time remaining: %d:%02.2d", minutes, seconds);
     }        
#endif

      return 1;
    }

  if ((num = check_player(who, 1)) == NONE) return 0;

  if (!(comm = strchr(comm, ' ')))
    {
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "Current game set to %d min with %d min overtime",
	       inl_stat.time/PERMIN, inl_stat.overtime/PERMIN);
      if ((inl_teams[HOME].time != inl_teams[AWAY].time ) ||
	  (inl_teams[HOME].overtime != inl_teams[AWAY].overtime))
	for (i=0; i<INLTEAM; i++)
	  pmessage(who, MINDIV, addr_mess(who, MINDIV),
		   "%s (%s) requests %d mins regulation and %d overtime",
		   inl_teams[i].t_name, inl_teams[i].name,
		   inl_teams[i].time, inl_teams[i].overtime);
      return 1;
    }

  if (!inl_stat.change)
    {			/* can't change it anymore */
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "You can no longer change the gametime. The game has started.");
      return 0;
    }

  if ((sscanf(comm,"%d %d",&time,&overtime))!=2)
    {
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "Two times are needed (regulation & overtime)");
      return 0;
    }

  inl_teams[num].time = time;
  inl_teams[num].overtime = overtime;

  if ((inl_teams[HOME].time != inl_teams[AWAY].time ) ||
      (inl_teams[HOME].overtime != inl_teams[AWAY].overtime))
    {
      pmessage(who, MALL, inl_from,
	       "%s (%s) has requested %d mins regulation and %d overtime",
	       inl_teams[num].t_name, inl_teams[num].name,
	       inl_teams[num].time, inl_teams[num].overtime);
    }
  else
    {
      pmessage(who, MALL, inl_from,
	       "A game of %d min with %d min overtime is approved.",
	       time, overtime);
      inl_stat.time = time * PERMIN;
      inl_stat.overtime = overtime * PERMIN;
    }
}


do_army(comm,mess)
     char *comm;
     struct message *mess;
{
  int who;
  int army;
  int num;
  int i;

#ifdef INLDEBUG
  ERROR(2,("	Enter do_setarmy\n"));
#endif

  who = mess->m_from;

  if ((num = check_player(who, 1)) == NONE) return 0;

  if (!(comm = strchr(comm, ' ')))
    {
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "The starting army count is set to %d.",
	       inl_stat.start_armies);
      if (inl_teams[HOME].start_armies != inl_teams[AWAY].start_armies)
	for (i=0; i<INLTEAM; i++)
	  pmessage(who, MINDIV, addr_mess(who, MINDIV),
		   "%s (%s) requests %d starting armies.",
		   inl_teams[i].t_name, inl_teams[i].name,
		   inl_teams[i].start_armies);
      return 1;
    }

  if (!inl_stat.change)
    {			/* can't change it anymore */
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "You can no longer change the starting armies. The game has started.");
      return 0;
    }

  if ((sscanf(comm,"%d",&army))!=1)
    {
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "Army takes exactly one argument");
      return 0;
    }

  inl_teams[num].start_armies = army;

  if (inl_teams[HOME].start_armies != inl_teams[AWAY].start_armies)
    {
      pmessage(who, MALL, inl_from,
	       "%s team (%s) requests starting armies be set to %d.",
	       inl_teams[num].t_name, inl_teams[num].name,
	       army);
    }
  else
    {
      pmessage(who, MALL, inl_from,
	       "Initial army number of %d approved.",
	       army);
      inl_stat.start_armies = army;
    }
}


/* Allows the captains of both teams to agree on resetting the galaxy */

do_resetgalaxy(comm,mess)
     char *comm;
     struct message *mess;
{
  int who;
  int c, num = -1;
  int rebuild = 0;

#ifdef INLDEBUG
  ERROR(2,("	Enter do_start\n"));
#endif

  who = mess->m_from;

  if (!inl_stat.change)
    {			/* can't change it anymore */
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "You can no longer reset the galaxy. The game has started.");
      return 0;
    }

  if ((num = check_player(who, 1)) == NONE) return 0;

  inl_teams[num].flags |= T_RESETGALAXY;

  pmessage(0, MALL, inl_from, "%s (%s) requests that the galaxy be rebuilt.",
	   inl_teams[num].t_name,
	   players[who].p_mapchars);

  for (c=0; c < INLTEAM; c++)
    {
      if (inl_teams[c].flags & T_RESETGALAXY) rebuild++;
    }

  if (rebuild != 2) return 1;

  for (c=0; c < INLTEAM; c++)
    {
      inl_teams[c].flags = 0;		/* reset all flags */
    }

  doResources(1);

  pmessage (0, MALL, inl_from,
	    "Game restarting with new galaxy.  Teams should be reselected");
}


/* Makes the player who sent the message captain if it hasn't been taken
   yet. */

do_captain(comm,mess)
     char *comm;
     struct message *mess;
{
  int who;
  int c, num=-1;

#ifdef INLDEBUG
  ERROR(2,("	Enter do_captain\n"));
#endif

  who = mess->m_from;

  if ((num = check_player(who, 0)) == NONE) return 0;

  if (inl_teams[num].captain == NONE)
    {
      inl_teams[num].captain = who;
      pmessage(0, MALL, inl_from, "%s (%s) is captain of %s team.",
	       players[who].p_name,
	       players[inl_teams[num].captain].p_mapchars,
	       inl_teams[num].name);
    }
  else if (players[inl_teams[num].captain].p_status != PALIVE)
    {
      pmessage (inl_teams[num].side, MTEAM, inl_from,
		"%s has died and been overthrown as captain!",
		players[inl_teams[num].captain].p_mapchars);
      inl_teams[num].captain = NONE;
    }
  else
    {
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "%s (%s) is captain of your team already.",
	       players[inl_teams[num].captain].p_name,
	       players[inl_teams[num].captain].p_mapchars);
      return 0;
    }

  return 1;
}


/* Allows the captain to release captaining duties */

do_uncaptain(comm,mess)
     char *comm;
     struct message *mess;
{
  int who;
  int c, num=-1;

#ifdef INLDEBUG
  ERROR(2,("	Enter do_uncaptain\n"));
#endif

  who = mess->m_from;

  if ((num = check_player(who, 1)) == NONE) return 0;

  inl_teams[num].captain = NONE;
  pmessage(0, MALL, inl_from, "%s (%s) relinquishes captain control of %s team.",
	   players[who].p_name, players[who].p_mapchars, inl_teams[num].name);

  return 1;
}


/* This allows the captain to pick the different races */

do_pickside(comm,mess)
     char *comm;
     struct message *mess;
{
  int who;

  int c, other_side, num=-1;
  int race,diagonal;

#ifdef INLDEBUG
  ERROR(2,("	Enter pickside\n"));
#endif

  who = mess->m_from;

  if (!inl_stat.change)
    {			/* can't change it anymore */
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "You can no longer pick a side. The game has started.");
      return 0;
    }


  if ((num = check_player(who, 1)) == NONE) return 0;

  for (c=0; c < INLTEAM; c++)
    {
      if (inl_teams[num].team != inl_teams[c].team)
	other_side = inl_teams[c].team;
    }

  if (inl_teams[num].flags & T_SIDELOCKED)
    {
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "Choice of sides is no longer available.");
      return 0;
    }

  if ((num == HOME) && (inl_teams[other_side].side_index == NOT_CHOOSEN))
    {
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "The away team needs to pick a side first");
      return 0;
    }

  if (strcmp(comm, "PASS") == 0)
    if (num == HOME)
      {
	pmessage(who, MINDIV, addr_mess(who, MINDIV),
		 " Hey!! Only the Away team captain uses this!");
	return 0;
      }
    else
      {
	inl_teams[num].side_index = RELINQUISH;
	pmessage(0, MALL, inl_from,
		 "Away team (%s) passes first race choice to home team!",
		 inl_teams[num].name);
      }
  else
    for (c = 0; c < NUMTEAM; c++)
      {
	if (strcmp(comm, sides[c].name) == 0)
	  {
	    if (inl_teams[other_side].side_index != NOT_CHOOSEN
		&& inl_teams[other_side].side_index != RELINQUISH)
	      {
		if (inl_teams[other_side].side_index == c)
		  {
		    pmessage(who, MINDIV, addr_mess(who, MINDIV),
			     "The %s team already choose %s",
			     inl_teams[other_side].name,
			     sides[inl_teams[other_side].side_index].name);
		    return 0;
		  }
		if (sides[inl_teams[other_side].side_index].diagonal == c)
		  {
		    pmessage(who, MINDIV, addr_mess(who, MINDIV),
			     "Cannot choose diagonal race to %s",
			     sides[inl_teams[other_side].side_index].name);
		    return 0;
		  }
		inl_teams[other_side].flags |= T_SIDELOCKED;
	      }

	    inl_teams[num].side_index = c;
	    inl_teams[num].name = strdup(sides[c].name);
	    pmessage(0, MALL, inl_from, "The %s team picks %s",
		     inl_teams[num].t_name,
		     sides[inl_teams[num].side_index].name);
	  }
      }
  return 1;
}

do_tname(comm,mess)
     char *comm;
     struct message *mess;
{
  int who;

  int c, other_side, num=-1;

#ifdef INLDEBUG
  ERROR(2,("	Enter do_tname\n"));
#endif
  who = mess->m_from;

  if (!inl_stat.change)
    {		/* can't change it anymore */
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "You can no longer change the team name. The game has started.");
      return 0;
    }


  if ((num = check_player(who, 1)) == NONE) return 0;

  if (!(comm = strchr(comm, ' ')))
    {
      pmessage(who, MINDIV, addr_mess(who, MINDIV), "No team name was given.");
      return 0;
    }

  if (strlen(comm)+1 < 2)
    {
      pmessage(who, MINDIV, addr_mess(who, MINDIV), "No team name was given.");
      return 0;
    }

  comm = ++comm;

  /*    free (inl_teams[num].t_name); */
  inl_teams[num].t_name = strdup (comm);
  pmessage(who, MINDIV, addr_mess(who, MINDIV),
	   "Team name set to %s", inl_teams[num].t_name);

  return 1;
}

int
end_pause()
{
  inl_stat.flags &= ~(S_FREEZE | S_COUNTDOWN);
  status->gameup &= ~(GU_PRACTICE | GU_PAUSED);
  pmessage(0,MALL, inl_from, "---- Game continues ----");
}

do_pause(comm,mess)
     char *comm;
     struct message *mess;
{
  int who;
  int queue;
  int side;
  struct player *j;
  int c, begin=0, num=-1;

#ifdef INLDEBUG
  ERROR(2,("	Enter do_pause\n"));
#endif

  who = mess->m_from;

  if ((num = check_player(who, 1)) == NONE) return 0;

  if (inl_stat.flags & S_PREGAME)
    {
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "You aren't even PLAYING!");
      return 0;
    }

  if (strcmp(comm, "PAUSE") == 0)
    {
      inl_teams[num].flags |= T_PAUSE;
      pmessage(0, MALL, inl_from, "%s (%s) requests a pause.",
	       inl_teams[num].t_name,
	       players[who].p_mapchars);
    }
  else if (strcmp(comm, "PAUSENOW") == 0)
    {
      for (c=0; c < INLTEAM; c++)
	inl_teams[c].flags |= T_PAUSE;
      pmessage(0, MALL, inl_from, "%s (%s) pauses the game.",
	       inl_teams[num].t_name,
	       players[who].p_mapchars);
    }
  else if (strcmp(comm, "CONTINUE") == 0)
    {
      if (!(inl_teams[num].flags & T_PAUSE))
	{
	  pmessage(who, MINDIV, addr_mess(who, MINDIV),
		   "Game is not paused.");
	  return 0;
	}
      inl_teams[num].flags &= ~T_PAUSE;
      pmessage(0, MALL, inl_from,
	       "%s (%s) requests that the game continue.",
	       inl_teams[num].t_name,
	       players[who].p_mapchars);
    }
  else
    {
      ERROR(1,("Unknown request passed to do_pause (%s)\n", comm));
      return 0;
    }

  for (c=0; c < INLTEAM; c++)
    {
      if (inl_teams[c].flags & T_PAUSE) begin++;
    }

  if (begin == 2)
    {
      inl_freeze();
    }
  else if (begin == 0)
    {
      inl_stat.flags |= S_COUNTDOWN;
      inl_countdown.idx=2;		/* 10 Second mark */
      inl_countdown.end=inl_stat.ticks+10*PERSEC;
      inl_countdown.action=end_pause;
      inl_countdown.message="Game continues in %i seconds";
    }

}

do_restart(comm,mess)
     char *comm;
     struct message *mess;
{
  int who;
  int queue;
  int side;
  struct player *j;
  int c, restart=0, num=-1;

#ifdef INLDEBUG
  ERROR(2,("	Enter do_restart\n"));
#endif

  who = mess->m_from;

  if ((num = check_player(who, 1)) == NONE) return 0;

  if (inl_stat.flags & S_PREGAME)
    {
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "You aren't even PLAYING!");
      return 0;
    }

  inl_teams[num].flags |= T_RESTART;
  pmessage(0, MALL, inl_from, "%s (%s) requests a RESTART.",
           inl_teams[num].t_name,
           players[who].p_mapchars);

  for (c=0; c < INLTEAM; c++)
    {
      if (inl_teams[c].flags & T_RESTART) restart++;
    }

  if (restart == 2)
    {
      pmessage(0, MALL, inl_from, "INL SERVER RESTARTED");
      reset_inl(0);
    }
}

do_timeout(char *comm, struct message *mess)
{
  int who;
  int other_side;
  int c, num=-1;

#ifdef INLDEBUG
  ERROR(2,("  Enter do_pause\n"));
#endif

  who = mess->m_from;

  if ((num = check_player(who, 1)) == NONE) return 0; /* captain? */

  if (inl_stat.flags & S_PREGAME)
    {
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "You aren't even PLAYING!");
      return 0;
    }

  for (c=0; c < INLTEAM; c++)
    {
      if (inl_teams[num].team != inl_teams[c].team)
	other_side = inl_teams[c].team;
    }

  if (inl_teams[num].flags & T_TIMEOUT)
    {
      inl_teams[num].flags &= ~(T_TIMEOUT);
      inl_teams[num].tmout++;

      if (!(inl_teams[other_side].flags & T_TIMEOUT))
	{
	  inl_stat.flags &= ~(S_TMOUT);
	  inl_stat.tmout_ticks = 0;
	}

      pmessage(0, MALL, inl_from,
	       "%s team (%s) has cancelled their timeout request!",
	       inl_teams[num].t_name,
	       players[who].p_mapchars);
    }
  else if (inl_teams[num].tmout <= 0)
    {
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "You have exhausted your timeout allowance for this game.");
    }
  else
    {
      inl_teams[num].flags |= T_TIMEOUT;
      inl_teams[num].tmout--;
      inl_stat.flags |= S_TMOUT;

      pmessage(0, MALL, inl_from,
	       "**********************************************************");
      pmessage(0, MALL, inl_from, "%s team (%s) has called a timeout!",
	       inl_teams[num].t_name,
	       players[who].p_mapchars);
      pmessage(0, MALL, inl_from,
	       "**********************************************************");
    }
}

do_confine(char *comm, struct message *mess)
{
  int who;
  int c, other_side, num=-1;

#ifdef INLDEBUG
  ERROR(2,("  Enter do_confine\n"));
#endif
  who = mess->m_from;

  if ((num = check_player(who, 1)) == NONE) return 0;

  if (!(inl_stat.flags & S_PREGAME))
    {
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "Weenie. You can't do a confine in tmode!!");
      return 0;
    }

  for (c=0; c < INLTEAM; c++)
    {
      if (inl_teams[num].team != inl_teams[c].team)
	other_side = inl_teams[c].team;
    }

  if (inl_teams[num].flags & T_CONFINE)
    {
      inl_teams[num].flags &= ~(T_CONFINE);

      if (!(inl_teams[other_side].flags & T_CONFINE))
	inl_stat.flags &= ~(S_CONFINE);

      pmessage(0, MALL, inl_from,
	       "%s team (%s) has toggled confine OFF (confine is an OR function)",
	       inl_teams[num].t_name,
	       players[who].p_mapchars);
    }
  else
    {
      inl_teams[num].flags |= T_CONFINE;
      inl_stat.flags |= S_CONFINE;

      pmessage(0, MALL, inl_from,
	       "%s team (%s) has toggled confine ON (confine is an OR function)",
	       inl_teams[num].t_name,
	       players[who].p_mapchars);
    }
}

do_free(char *comm, struct message *mess)
{
  int num=-1;
  int victim;
  char slot;
  struct player *j;
  int who = mess->m_from;

  if ((num = check_player(who, 1)) == NONE) return 0; /* Captain ? */

  if ((victim = getplayer(who, comm)) == -1)
    return 0;

#ifdef nodef
  /* if the captain tries to free a slot on the other team, deny it */
  if (
	( players[who].w_queue == QU_HOME && 
	  players[victim].w_queue != QU_HOME &&
	  players[victim].w_queue != QU_HOME_OBS ) ||
	( players[who].w_queue == QU_AWAY &&
	  players[victim].w_queue != QU_AWAY &&
	  players[victim].w_queue != QU_AWAY_OBS )
     )
    {
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
               "That slot is on the other team.");
      return 1;
    }
#endif

  j = &players[victim];
  if (j->p_status==PFREE)
    {
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
	       "Slot is already free.");
      return 1;
    }
 
  /* convert slot # to slot character */ 
  slot = (victim < 10) ? '0' + victim : 'a' + (victim - 10);

  /* save some SB armies, but don't save normal ship armies so that
     captains can't free-scum.  -da */

  if ((j->p_ship.s_type == STARBASE) && (j->p_armies > 0)
      && (!(j->p_flags & PFOBSERV))) {
    int i, k;

    k=10*(remap[j->p_team]-1);

    if (k>=0 && k<=30) {
      for (i=0; i<10; i++) {
        if (planets[i+k].pl_owner == j->p_team) {
          planets[i+k].pl_armies += j->p_armies;
          pmessage(0, MALL | MGHOST, "GOD->ALL",
            "%s's %d armies placed on %s",
            j->p_name, j->p_armies, planets[k+i].pl_name);
          break;
        }
      }
    }
  }

  if (j->p_process)
    {
      kill(j->p_process, SIGTERM);
      pmessage(0, MALL, inl_from, "Freeing slot %c ", slot);
      return 1;
    }

  /* paranoia */
  freeslot(j);
  pmessage(0, MALL, inl_from, "Freed slot %c ", slot);
  return 1;
}

void announce_scores(int who, int flag, FILE *fp) {
  int win_cond;
  char *h_name, *a_name;
  char *p_ahead, *c_ahead, *m_ahead;
  char *s_mode;
  float h_cont, a_cont;
  int h_planets, a_planets, n_planets;

  if (inl_stat.game_ticks < 1)
    return;

  if (inl_stat.weighted_divisor < 1.0)
    return;

  /* this is critical! */
  win_cond = check_winner();

  h_name = sides[inl_teams[HOME].side_index].name;
  a_name = sides[inl_teams[AWAY].side_index].name;

  h_cont = (float) inl_teams[HOME].weighted_score / inl_stat.weighted_divisor;
  a_cont = (float) inl_teams[AWAY].weighted_score / inl_stat.weighted_divisor;

  h_planets = inl_teams[HOME].planets;
  a_planets = inl_teams[AWAY].planets;
  n_planets = 20 - h_planets - a_planets;

  if (h_planets > a_planets + 2)
    p_ahead = h_name;
  else if (a_planets > h_planets + 2)
    p_ahead = a_name;
  else
    p_ahead = "tied";

  if (h_cont > a_cont + 2.0)
    c_ahead = h_name;
  else if (a_cont > h_cont + 2.0)
    c_ahead = a_name;
  else
    c_ahead = "tied";

  if ((h_planets > a_planets + 2) || (h_cont > a_cont + 2.0))
    m_ahead = h_name;
  else if ((a_planets > h_planets + 2) || (a_cont > h_cont + 2.0))
    m_ahead = a_name;
  else
    m_ahead = "tied";

  pmessage(who, flag, addr_mess(who, flag),
           "Planet count: %s - %s:  %i - %i - %i  [%s]",
           h_name, a_name, h_planets, a_planets, n_planets, p_ahead);

  pmessage(who, flag, addr_mess(who, flag),
           "Continuous score: %s - %s:  %.2f - %.2f  [%s]",
           h_name, a_name, h_cont, a_cont, c_ahead);

  if (fp) {
    fprintf(fp, "SCORE: Planet count: %s - %s:  %i - %i - %i  [%s]\n",
            h_name, a_name, h_planets, a_planets, n_planets, p_ahead);

    fprintf(fp, "SCORE: Continuous score: %s - %s:  %.2f - %.2f  [%s]\n",
            h_name, a_name, h_cont, a_cont, c_ahead);
  }

  switch(inl_stat.score_mode) {
    case 0:
      s_mode = "planet count";
      break;
    case 1:
      s_mode = "continuous score";
      break;
    case 2:
      s_mode = "momentum score";
      break;
    default:
      s_mode = "UNKNOWN (report this bug)";
      break;
  }

  switch(win_cond) {
    case -1:
    case 0:
      pmessage(who, flag, addr_mess(who, flag), "Game TIED by %s", s_mode);
      if (fp) fprintf(fp, "SCORE: Game TIED by %s\n", s_mode);
      break;
    case 1:
      pmessage(who, flag, addr_mess(who, flag), "%s winning by %s", p_ahead, s_mode);
      if (fp) fprintf(fp, "SCORE: %s winning by %s\n", p_ahead, s_mode);
      break;
    case 2:
      pmessage(who, flag, addr_mess(who, flag), "%s winning by %s", c_ahead, s_mode);
      if (fp) fprintf(fp, "SCORE: %s winning by %s\n", c_ahead, s_mode);
      break;
    case 3:
      pmessage(who, flag, addr_mess(who, flag), "%s winning by %s [tie break]",
               p_ahead, s_mode);
      if (fp) fprintf(fp, "SCORE: %s winning by %s [tie break]\n", p_ahead, s_mode);
      break;
    case 4:
      pmessage(who, flag, addr_mess(who, flag), "%s winning by %s",
               m_ahead, s_mode);
      if (fp) fprintf(fp, "SCORE: %s winning by %s\n", m_ahead, s_mode);
      break;
  }

}

int do_cscore(char *comm, struct message *mess) {

  announce_scores(mess->m_from, MINDIV, NULL);

  return 1;
}

int do_scoremode(char *comm, struct message *mess) {

  int who, team;
  char *mode;
  int sc_mode = -1;

  who = mess->m_from;

  if (!inl_stat.change) {
    pmessage(who, MINDIV, addr_mess(who, MINDIV),
             "Can't switch scoring modes.  The game has started.");
    return 0;
  }

  team = check_player(who, 1);

  comm = strchr(comm, ' ');

  if (comm && (sscanf(comm, "%d", &sc_mode) != 1))
    sc_mode = -1;

  switch(sc_mode) {
    case 0:
      inl_teams[team].score_mode = 0;
      mode = "NORMAL";
      break;
    case 1:
      inl_teams[team].score_mode = 1;
      mode = "CONTINUOUS";
      break;
    case 2:
      inl_teams[team].score_mode = 2;
      mode = "MOMENTUM";
      break;
    default:
      pmessage(who, MINDIV, addr_mess(who, MINDIV),
               "Usage: SCOREMODE [012]; 0=planets, 1=continuous; 2=momentum");
      return 0;
  }

  pmessage(0, MALL, inl_from, "%s requests %s scoring mode.",
           players[who].p_mapchars, mode);

  if (inl_teams[HOME].score_mode == inl_teams[AWAY].score_mode) {
    pmessage(0, MALL, inl_from, "%s scoring mode approved.  See MOTD.", mode);
    inl_stat.score_mode = inl_teams[HOME].score_mode;
  }
}

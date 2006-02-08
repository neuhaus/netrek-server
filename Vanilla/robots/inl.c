/* 	$Id: inl.c,v 1.1 2005/03/21 05:23:46 jerub Exp $	 */

#ifndef lint
static char vcid[] = "$Id: inl.c,v 1.1 2005/03/21 05:23:46 jerub Exp $";
#endif /* lint */

/*
 * inl.c
 *
 * INL server robot by Kurt Siegl
 * Based on basep.c
 *
 * Many improvements by Kevin O'Connor
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <errno.h>
#include <pwd.h>
#include <ctype.h>
#include <time.h>
#include <stdlib.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "planets.h"
#include "inldefs.h"
#include INC_STRINGS
#include "proto.h"
#include "ltd_stats.h"

/*

From: Tom Holub <doosh@best.com>
Date: Fri, 27 Apr 2001 20:37:06 -0700
Message-ID: <20010427203706.A17757@best.com>

Default regulation and overtime periods

*/
#define INL_REGULATION 60
#define INL_OVERTIME 20

int debug=0;

static int cambot_pid = 0;

struct planet *oldplanets;	/* for restoring galaxy */
#ifdef nodef
struct planet *inl_planets;
#endif
struct itimerval udt;

int	oldmctl;
FILE	*inl_log;

char time_msg[4][10] = {"seconds", "second", "minutes", "minute"};

Team	inl_teams[INLTEAM+1] = {
  {
    HOME,		/* Away or home team */
    "home",		/* away or home string */
    "Home",		/* Name of team */
    NONE,		/* who is captain */
    HOMETEAM,		/* Race flag */
    NOT_CHOOSEN,	/* index into side array */
    17,			/* armies requested to start with */
    INL_REGULATION,	/* requested length of game */
    INL_OVERTIME,	/* requested length of overtime */
    0,			/* team flags */
    1,			/* tmout counter */
    10,			/* number of planets owned */
    0, 0, 0		/* continuous scores */

  },
  {
    AWAY,		/* Away or home team */
    "away",		/* away or home string */
    "Away",		/* Name of team */
    NONE,		/* who is captain */
    AWAYTEAM,		/* Race flag */
    NOT_CHOOSEN,	/* index into side array */
    17,			/* armies requested to start with */
    INL_REGULATION,	/* requested length of game */
    INL_OVERTIME,	/* requested length of overtime */
    0,			/* team flags */
    1,			/* tmout counter */
    10,			/* number of planets owned */
    0, 0, 0		/* continuous scores */
  }
};

Sides	sides[MAXTEAM+1] = { {"FED", FED, 2},
			     {"ROM", ROM, 3},
			     {"KLI", KLI, 0},
			     {"ORI", ORI, 1}};

int cbounds[9][4] =
{
  {0, 0, 0, 0},
  {0, 45000, 55000, 100000},
  {0, 0, 55000, 55000},
  {0, 0, 0, 0},
  {45000, 0, 100000, 55000},
  {0, 0, 0, 0},
  {0, 0, 0, 0},
  {0, 0, 0, 0},
  {45000, 45000, 100000, 100000}
};

char	*roboname = "INL";
char	*inl_from = {"INL->ALL"};

Inl_stats inl_stat = {
  17,			/* start_armies */
  1,			/* change */
  S_PREGAME,		/* flags */
  0,			/* ticks */
  0,			/* game_ticks */
  0,			/* tmout_ticks */
  0,			/* time */
  0,			/* overtime */
  0,			/* extratime */
  0,			/* score_mode */
  0.0			/* weighted_divisor */
};

Inl_countdown inl_countdown =
{0,{60,30,10,5,4,3,2,1,0,-1},8,0,PERSEC,NULL,""};

int end_tourney();
int checkgeno();
Inl_countdown inl_game =
{0,{2700,1800,600,300,120,60,30,10,5,4,3,2,1,0,-1},12,0,PERSEC,end_tourney,""};

/* XXX Gag me! */
/* the four close planets to the home planet */
static int core_planets[4][4] =
{
  {7, 9, 5, 8,},
  {12, 19, 15, 16,},
  {24, 29, 25, 26,},
  {34, 39, 38, 37,},
};
/* the outside edge, going around in order */
static int front_planets[4][5] =
{
  {1, 2, 4, 6, 3,},
  {14, 18, 13, 17, 11,},
  {22, 28, 23, 21, 27,},
  {31, 32, 33, 35, 36,},
};

void cleanup();
void reset_inl(int);
int checkmess();
void inlmove();
int start_tourney();
void reset_stats();
void update_scores();
void announce_scores(int, int, FILE *);

extern char *addr_mess(int who, int type);


int
main(argc, argv)
     int	     argc;
     char	    *argv[];
{
  int i;

  srandom(time(NULL));

  getpath();
  openmem(1);
  readsysdefaults();

  if ((inl_log = fopen(N_INLLOG,"w+"))==NULL) {
    ERROR(1,("Could not open INL log file.\n"));
    exit(1);
  }

  SIGNAL(SIGALRM, inlmove);		/*the def signal is needed - MK */
  if (!debug)
    {
      SIGNAL(SIGINT, cleanup);
      SIGNAL(SIGTERM, cleanup);
    }

  oldmctl = mctl->mc_current;
#ifdef nodef
  for (i = 0; i <= oldmctl; i++) {
    check_command(&messages[i]);
  }
#endif

  pmessage(0, MALL, inl_from, "###############################");
  pmessage(0, MALL, inl_from, "#  The INL robot has entered  #");
  pmessage(0, MALL, inl_from, "###############################");

  init_server();
  reset_inl(0);

  udt.it_interval.tv_sec = 0;	     /* Robots move PERSEC times/sec */
  udt.it_interval.tv_usec = 1000000 / PERSEC;

  udt.it_value.tv_sec = 1;
  udt.it_value.tv_usec = 0;
  if (setitimer(ITIMER_REAL, &udt, 0) < 0) {
    perror("setitimer");
    exit(1);
  }

  /* allows robots to be forked by the daemon -- Evil ultrix bullshit */
  SIGSETMASK(0);

  while (1) {
    PAUSE(SIGALRM);
  }
}

void
inl_freeze(void)
{
  int c;
  struct player *j;

  for (c=0; c < INLTEAM; c++)
    inl_teams[c].flags |= T_PAUSE;

  inl_stat.flags |= S_FREEZE;
  status->gameup |= (GU_PRACTICE | GU_PAUSED);

  for (j = firstPlayer; j <= lastPlayer; j++)
    {
      j->p_flags &= ~(PFSEEN);
      j->p_lastseenby = VACANT;	 /* if left non-vacant the daemon might */
    }				 /* not reinitalize PFSEEN automaticly */

  pmessage(0, MALL, inl_from, "-------- Game is paused -------");

}

void
inl_confine(void)
{
  struct player *j;

  for (j = firstPlayer; j <= lastPlayer; j++)
    {
      if (j->p_x < cbounds[j->p_team][0])
	j->p_x = cbounds[j->p_team][0];
      if (j->p_y < cbounds[j->p_team][1])
	j->p_y = cbounds[j->p_team][1];
      if (j->p_x > cbounds[j->p_team][2])
	j->p_x = cbounds[j->p_team][2];
      if (j->p_y > cbounds[j->p_team][3])
	j->p_y = cbounds[j->p_team][3];
    }
}

typedef struct playerlist {
    char name[NAME_LEN];
    char mapchars[2];
    int planets, armies;
} Players;


static void displayBest(FILE *conqfile, int team, int type)
{
    register int i,k,l;
    register struct player *j;
    int planets, armies;
    Players winners[MAXPLAYER+1];
    char buf[MSG_LEN];
    int number;

    number=0;
    MZERO(winners, sizeof(Players) * (MAXPLAYER+1));
    for (i = 0, j = &players[0]; i < MAXPLAYER; i++, j++) {
        if (j->p_team != team || j->p_status == PFREE) continue;
#ifdef GENO_COUNT
        if (type==KWINNER) j->p_stats.st_genos++;
#endif
        if (type == KGENOCIDE) {
            planets=j->p_genoplanets;
            armies=j->p_genoarmsbomb;
            j->p_genoplanets=0;
            j->p_genoarmsbomb=0;
        } else {
            planets=j->p_planets;
            armies=j->p_armsbomb;
        }
        for (k = 0; k < number; k++) {
            if (30 * winners[k].planets + winners[k].armies < 
                30 * planets + armies) {
	      /* insert person at location k */
                break;
            }
        }
        for (l = number; l >= k; l--) {
            winners[l+1] = winners[l];
        }
        number++;
        winners[k].planets=planets;
        winners[k].armies=armies;
        STRNCPY(winners[k].mapchars, j->p_mapchars, 2);
        STRNCPY(winners[k].name, j->p_name, NAME_LEN);
        winners[k].name[NAME_LEN-1]=0;  /* `Just in case' paranoia */
    }
    for (k=0; k < number; k++) {
      if (winners[k].planets != 0 || winners[k].armies != 0) {
            sprintf(buf, "  %16s (%2.2s) with %d planets and %d armies.", 
                winners[k].name, winners[k].mapchars, winners[k].planets, winners[k].armies);
            pmessage(0, MALL | MCONQ, " ",buf);
            fprintf(conqfile, "  %s\n", buf);
      }
    }
    return;
}


static void genocideMessage(int loser, int winner)
{
    char buf[MSG_LEN];
    FILE *conqfile;
    time_t curtime;

    conqfile=fopen(ConqFile, "a");

    /* TC 10/90 */
    time(&curtime);
    strcpy(buf,"\nGenocide! ");
    strcat(buf, ctime(&curtime));
    fprintf(conqfile,"  %s\n",buf);

    pmessage(0, MALL | MGENO, " ","%s",
        "***********************************************************");
    sprintf(buf, "%s has been genocided by %s.", inl_teams[loser].t_name,
       inl_teams[winner].t_name);
    pmessage(0, MALL | MGENO, " ","%s",buf);
        
    fprintf(conqfile, "  %s\n", buf);
    sprintf(buf, "%s:", inl_teams[winner].t_name);
    pmessage(0, MALL | MGENO, " ","%s",buf);
    fprintf(conqfile, "  %s\n", buf);
    displayBest(conqfile, inl_teams[winner].side, KGENOCIDE);
    sprintf(buf, "%s:", inl_teams[loser].t_name);
    pmessage(0, MALL | MGENO, " ","%s",buf);
    fprintf(conqfile, "  %s\n", buf);
    displayBest(conqfile, inl_teams[loser].side, KGENOCIDE);
    pmessage(0, MALL | MGENO, " ","%s",
        "***********************************************************");
    fprintf(conqfile, "\n");
    fclose(conqfile);
}


int 
checkgeno()
{
    register int i;
    register struct planet *l;
    struct planet *homep = NULL;
    struct player *j;
    int loser, winner;

    for (i = 0; i <= MAXTEAM; i++) /* maint: was "<" 6/22/92 TC */
        teams[i].s_plcount = 0;

    for (i = 0, l = &planets[i]; i < MAXPLANETS; i++, l++) {
        teams[l->pl_owner].s_plcount++; /* for now, recompute here */
    }

    if (teams[inl_teams[HOME].side].s_plcount == 0) 
      {
	loser = HOME;
	winner = AWAY;
      }
    else if (teams[inl_teams[AWAY].side].s_plcount == 0)
           {       
	     loser = AWAY;
	     winner = HOME;
	   }
         else
           return 0;


    /* Tell winning team what wonderful people they are (pat on the back!) */
    genocideMessage(loser, winner);
    /* Give more troops to winning team? */

    /* kick out all the players on that team */
    /* makes it easy for them to come in as a different team */
    for (i = 0, j = &players[0]; i < MAXPLAYER; i++, j++) {
      if (j->p_status == PALIVE && j->p_team == loser) {
	j->p_status = PEXPLODE;
	if (j->p_ship.s_type == STARBASE)
	  j->p_explode = 2*SBEXPVIEWS/PLAYERFUSE;
	else
	  j->p_explode = 10/PLAYERFUSE;
	j->p_whydead = KGENOCIDE;
	j->p_whodead = inl_teams[winner].captain;
      }
    }
    return 1;
}

void
inlmove()
{
  int c;

  /***** Start The Code Here *****/

  /* Don't tell us it's time for another move in the middle of a move. */
  SIGNAL(SIGALRM, SIG_IGN);

#ifdef INLDEBUG
  ERROR(2,("Enter inlsmove\n"));
#endif

  inl_stat.ticks++;
  player_maint();		      /* update necessary stats and info */

  if (inl_stat.flags & S_COUNTDOWN )
    countdown(inl_stat.ticks,&inl_countdown);
  else					/* no com. during countdown */
    checkmess();

  if (inl_stat.flags & S_CONFINE)
    inl_confine();

  if (!(inl_stat.flags & (S_TOURNEY | S_OVERTIME)) ||
      (inl_stat.flags & S_FREEZE )) {
    SIGNAL(SIGALRM, inlmove);
    return;
  }

  inl_stat.game_ticks++;

  /* check for timeout */
  if (inl_stat.flags & S_TMOUT)
    {
      inl_stat.tmout_ticks++;
      if ((inl_stat.tmout_ticks > 1300) || all_alert(PFGREEN)
	  || ((inl_stat.tmout_ticks > 800) && all_alert(PFYELLOW)))
	/* This is a loose approximation
	   of the INL source code.. */
	{
	  inl_stat.flags &= ~(S_TMOUT);
	  inl_stat.tmout_ticks = 0;

	  for (c=0; c < INLTEAM; c++)
	    inl_teams[c].flags &= ~(T_TIMEOUT);

	  inl_freeze();
	}
    }

  status->tourn = 1;		/* paranoia? */
  tournplayers = 1;
#ifdef nodef
  if (!(inl_stat.game_ticks % PLANETFUSE))
    checkplanets();
#endif

  /* check for sudden death overtime */
  if ((inl_stat.flags & S_OVERTIME) && (inl_stat.game_ticks % PERSEC))
    end_tourney();

  /* display periodic countdown */
  if (!(inl_stat.game_ticks % TIMEFUSE))
    countdown(inl_stat.game_ticks, &inl_game);

  /* update continuous scoring */
  update_scores();

  /* check for geno and end game if there is */
  if (checkgeno()) end_tourney();

#ifdef INLDEBUG
  ERROR(2,("	game ticks = %d\n", inl_stat.game_ticks));
#endif

  SIGNAL(SIGALRM, inlmove);
}

player_maint()
{
#ifdef INLDEBUG
  ERROR(2,("Enter player_maint\n"));
#endif
}

int
all_alert(int stat)
{
  struct player *j;

  for (j = firstPlayer; j <= lastPlayer; j++)
    if ((j->p_status == PALIVE) && !(j->p_flags & stat))
      return 0;

  return 1;
}

log(m)
     struct message *m;
{
  time_t curtime;
  struct tm *tmstruct;
  int hour;
  int least = MBOMB;
  /* decide whether or not to log this message */
#ifdef nodef
  if (m->m_flags & MINDIV) return; /* individual message */
  if (!(m->m_flags & MGOD)) return;
  if ((m->m_flags & MGOD) > least) return;
#endif
  /*
    time(&curtime);
    tmstruct = localtime(&curtime);
    if (!(hour = tmstruct->tm_hour%12)) hour = 12;
    fprintf(inl_log,"%02d:%02d %-73.73s\n", hour, tmstruct->tm_min,
    m->m_data);
    */
  fprintf(inl_log,"%5d: %s\n",inl_stat.ticks,m->m_data);
}

checkmess()
{
  int	shmemKey = PKEY;
  int i;

#ifdef INLDEBUG
  ERROR(2,("Enter checkmess\n"));
#endif

  /* make sure shared memory is still valid */
  if (shmget(shmemKey, SHMFLAG, 0) < 0) {
    exit(1);
    ERROR(2,("ERROR: Invalid shared memory\n"));

  }

  while (oldmctl!=mctl->mc_current) {
    oldmctl++;
    if (oldmctl==MAXMESSAGE) oldmctl=0;
    if (messages[oldmctl].m_flags & MINDIV) {
      if (messages[oldmctl].m_recpt == messages[oldmctl].m_from) {
	me = &players[messages[oldmctl].m_from];
	if (!check_command(&messages[oldmctl]))
	  pmessage(messages[oldmctl].m_from, MINDIV,
		   addr_mess(messages[oldmctl].m_from, MINDIV),
		   "Not an INL command.  Send yourself 'help' for help.");
      }
    }
#ifdef nodef
    else if (messages[oldmctl].m_flags & MALL) {
      if (strstr(messages[oldmctl].m_data, "help") != NULL)
	pmessage(messages[oldmctl].m_from, MINDIV,
		 addr_mess(messages[oldmctl].m_from, MINDIV),
		 "If you want help, send the message 'help' to yourself.");
    }
#endif
#ifdef RCD
    if (messages[oldmctl].m_flags == (MTEAM | MDISTR | MVALID)) {
      struct message msg;
      int size;
      struct distress dist;
      char buf[MSG_LEN];

      MCOPY(&messages[oldmctl],&msg,sizeof(struct message));
      buf[0]='\0';
      msg.m_flags ^= MDISTR;
      HandleGenDistr(msg.m_data,msg.m_from,msg.m_recpt,&dist);
      size = makedistress(&dist,buf,distmacro[dist.distype].macro);
      strncpy(msg.m_data,buf,size+1);
      log(&(msg));
    } else {
      log(&(messages[oldmctl]));
    }
#else
    log(&(messages[oldmctl]));
#endif
  }
}

void
countplanets()
{
  int i, c;

  for (i=0; i < INLTEAM; i++)
    inl_teams[i].planets = 0;
  for (c=0; c < MAXPLANETS; c++ )
    for (i=0; i < INLTEAM; i++)
      if (sides[inl_teams[i].side_index].flag == planets[c].pl_owner)
	inl_teams[i].planets++;
}

/* continuous scoring    -da */
void update_scores() {

  int p, t;
  double weight = (double) inl_stat.game_ticks / (double) inl_stat.time;

  static const double weight_max = 2.0;  /* exp(1.0) - 0.71828183 */

  /* sanity check! */
  if (weight < 0.0)
    weight = 0.0;
  else if (weight > 1.0)
    weight = 1.0;

  /* weight = linear range between 0.0 and 1.0 during regulation.
   * map this to exponential curve using formula:
   *    weight = exp(time) - (0.71828183 * time)
   *    weight range is 1.0 to 2.0
   * -da */

  weight = exp(weight) - (0.71828183 * weight);

  /* sanity check */
  if (weight < 1.0)
    weight = 1.0;
  else if (weight > weight_max)
    weight = weight_max;

  /* weight multiply factor should be max points you can get per planet */
  inl_stat.weighted_divisor += (weight * 2.0);

  for (p=0; p<MAXPLANETS; p++) {

    for(t=0; t<INLTEAM; t++) {

      if (sides[inl_teams[t].side_index].flag == planets[p].pl_owner) {

        /* planet is owned by this team, so it has > 0 armies.  each planet
           is worth 1-2 points for each pair of armies on the planet,
           capped at 2.  I.e. 1-2 armies = 1 pt, >=3 armies = 2 pts. */

        int points = planets[p].pl_armies;

        if (points > 2)
          points = 2;
        else
          points = 1;

        /* absolute continuous score is just the cumulative point total */
        inl_teams[t].abs_score += points;

        /* semi-continuous score is just the cumulative point total with
           a 1 minute interval */
        if ((inl_stat.game_ticks % PERMIN) == 0)
          inl_teams[t].semi_score += points;

        /* weighted continuous score is a cumulative total of points
           measured in a linearly decreasing interval */
        inl_teams[t].weighted_score += (weight * (double) points);

      }

    }

  }

  /* announce updated scores every 5 minutes if not in normal mode */

  if (inl_stat.score_mode && ((inl_stat.game_ticks % (5 * PERMIN)) == 0))
    announce_scores(0, MALL, NULL);

}

#ifdef nodef
checkplanets()
{
  int c;
  int size;
  int i;

#ifdef INLDEBUG
  ERROR(2,("Enter checkplanets\n"));
#endif

  size = sizeof(struct planet);

  for (c=0; c < MAXPLANETS; c++ ) {
    if (MCMP(&inl_planets[c], &planets[c], size) != 0) {
      /* Some planet information changed. Find out what it is and
	 report it. */

      /* pl_no, & pl_flags should never change */
      /* pl_x, pl_y, pl_name, pl_namelen should never change in INL */
      /* pl_deadtime & pl_couptime should not be of any interest */

      if (inl_planets[c].pl_owner != planets[c].pl_owner) {
	fprintf(inl_log,"PLANET: %s was taken by the %s from %s\n",
		inl_planets[c].pl_name, teamshort[planets[c].pl_owner],
		teamshort[inl_planets[c].pl_owner]);

	/*  Keep team[x].planets up to date */
	for (i=0; i < INLTEAM; i++)
	  {
	    if (sides[inl_teams[i].side_index].flag
		== inl_planets[c].pl_owner)
	      inl_teams[i].planets--;
	    if (sides[inl_teams[i].side_index].flag
		== planets[c].pl_owner)
	      inl_teams[i].planets++;
	  }

	inl_planets[c].pl_owner = planets[c].pl_owner;
      }
      if (inl_planets[c].pl_armies != planets[c].pl_armies) {
	if (inl_planets[c].pl_armies > planets[c].pl_armies)
	  fprintf(inl_log,
		  "PLANET: %s's armies have been reduced to %d\n",
		  inl_planets[c].pl_name,
		  planets[c].pl_armies);
	else
	  fprintf(inl_log,
		  "PLANET: %s's armies have increased to %d\n",
		  inl_planets[c].pl_name,
		  planets[c].pl_armies);
	inl_planets[c].pl_armies = planets[c].pl_armies;
      }
      if (inl_planets[c].pl_info != planets[c].pl_info) {
	int who, i;

	who = inl_planets[c].pl_info | planets[c].pl_info &
	  ~(inl_planets[c].pl_info & planets[c].pl_info);

	for (i = 0; i < INLTEAM; i++) {
	  if (who & sides[inl_teams[i].side_index].flag)
	    fprintf(inl_log,
		    "PLANET: %s has been touched by the %ss(%s)\n",
		    inl_planets[c].pl_name,
		    sides[inl_teams[i].side_index].name,
		    inl_teams[i].name);
	}
	inl_planets[c].pl_info = planets[c].pl_info;
      }
    }
  }
}
#endif

/* Determine if there is a winner at the end of regulation.
   returns -1 if momentum score goes to EXTRA TIME
   returns  0 if game is tied
   returns  1 if winner by planet count
   returns  2 if winner by continuous score > 2.0
   returns  3 if winner by continuous score < 2.0 & planet 11-8-1
   returns  4 if winner by momentum scoring
   -da
 */
int check_winner() {
  double divisor = inl_stat.weighted_divisor;
  double delta;

  /* -1 = HOME is winning
   *  0 = tied
   *  1 = AWAY is winning */
  int winning_cont = 0;
  int winning_norm = 0;

  countplanets();

  if (inl_teams[HOME].planets > inl_teams[AWAY].planets + 2)
    winning_norm = -1;
  else if (inl_teams[AWAY].planets > inl_teams[HOME].planets + 2)
    winning_norm = 1;
  else
    winning_norm = 0;

  /* NORMAL scoring mode; OR, continuous/momentum scoring and in OT.
     Use absolute planet count. */

  if ((inl_stat.score_mode == 0) || (inl_stat.flags & S_OVERTIME)) {
    if (winning_norm)
      return 1;
    else
      return 0;
  }

  /* CONTINUOUS or MOMENTUM scoring mode */

  /* sanity check */
  if (inl_stat.weighted_divisor < 1.0)
    return 0;

  if (inl_stat.game_ticks < 100)
    return 0;

  delta = inl_teams[HOME].weighted_score / divisor -
          inl_teams[AWAY].weighted_score / divisor;

  /* 2.0 is the differential */
  if (delta >= 2.0)
    winning_cont = -1;
  else if (delta <= -2.0)
    winning_cont = 1;
  else
    winning_cont = 0;

  /* CONTINUOUS scoring mode:
     delta > 2.0: win
     delta < 2.0 but planet > 11-8-1: win
     otherwise, tie.
   */

  if (inl_stat.score_mode == 1) {

    if (winning_cont)
      return 2;
    else if (winning_norm)
      return 3;
    else
      return 0;

  }

  /* MOMENTUM scoring mode */
  else if (inl_stat.score_mode == 2) {

    /* game tied, go to OT */
    if (!winning_cont && !winning_norm)
      return 0;

    /* if either team is winning by at least one method, return winner */
    if (winning_cont + winning_norm)
      return 4;

    /* otherwise, EXTRA TIME */
    else
      return -1;

  }

}

int
end_tourney()
{
  int game_over = 0;
  int win_cond;

  /* check_winner always returns > 0 if winner exists */
  if ((win_cond = check_winner()) > 0)
    {
      pmessage(0, MALL, inl_from, "---------- Game Over ----------");

      /* need to write this to the inl_log because the pmessages don't
         get flushed out to the log before the close */
      fprintf(inl_log, "---------- Game Over ---------\n");

      announce_scores(0, MALL, inl_log);

      switch(win_cond) {
        case 1:
          pmessage(0, MALL, inl_from, "Victory by planet count");
          fprintf(inl_log, "SCORE: Victory by planet count\n");
          break;
        case 2:
          pmessage(0, MALL, inl_from, "Victory by continuous score >= 2.0");
          fprintf(inl_log, "SCORE: Victory by continuous score >= 2.0\n");
          break;
        case 3:
          pmessage(0, MALL, inl_from, "Victory by planet count (continuous score < 2.0)");
          fprintf(inl_log, "SCORE: Victory by planet count (continuous score < 2.0)\n");
          break;
        case 4:
          pmessage(0, MALL, inl_from, "Victory by momentum score");
          fprintf(inl_log, "SCORE: Victory by momentum score\n");
          break;
        default:
          pmessage(0, MALL, inl_from, "Victory by UNKNOWN");
          fprintf(inl_log, "SCORE: Victory by UNKNOWN\n");
          break;
      }

      game_over = 1;
    }


  /* still in regulation, but momentum scoring dictates EXTRA TIME */
  else if ((inl_stat.flags & S_TOURNEY) && (win_cond == -1))
    {

      static const int extratime = 5 * PERMIN;
      static const int extra_max = 5 * PERMIN * 3;

      /* only allow 3 cycles of extra time */
      if (inl_stat.extratime < extra_max) {

        pmessage(0, MALL, inl_from, "---------- Extra Time (Momentum) ----------");
        pmessage(0, MALL, inl_from, "%i of %i total minutes added to regulation.",
                 extratime / PERMIN, extra_max);

        fprintf(inl_log, "---------- Extra Time (Momentum) ----------\n");
        fprintf(inl_log, "%i of %i total minutes added to regulation\n",
                extratime / PERMIN, extra_max);

        /* first extra time cycle results in obliteration */
        if (inl_stat.extratime == 0)
          obliterate(0,KPROVIDENCE);

        inl_stat.extratime += extratime;

        inl_game.idx = 0;
        inl_game.end += extratime;
        inl_game.message = "%i %s left in EXTRA time (momentum)";

        announce_scores(0, MALL, inl_log);

      }

      /* after extra time is over, declare continuous score the winner */
      else {
        pmessage(0, MALL, inl_from, "---------- Game Over (Momentum) ----------");

        fprintf(inl_log, "---------- Game Over (Momentum) ----------\n");

        announce_scores(0, MALL, inl_log);

        if (inl_teams[0].weighted_score > inl_teams[1].weighted_score) {
          pmessage(0, MALL, inl_from, "Tie breaker: %s wins by continuous score",
                   sides[inl_teams[0].side_index].name);
          fprintf(inl_log, "Tie breaker: %s wins by continuous score\n",
                  sides[inl_teams[0].side_index].name);
        }
        else {
          pmessage(0, MALL, inl_from, "Tie breaker: %s wins by continuous score",
                   sides[inl_teams[1].side_index].name);
          fprintf(inl_log, "Tie breaker: %s wins by continuous score\n",
                  sides[inl_teams[1].side_index].name);
        }

        game_over = 1;
      }

    }

  else if (inl_stat.flags & S_TOURNEY)
    {
      inl_stat.flags &= ~(S_TOURNEY | S_COUNTDOWN);
      inl_stat.flags |= S_OVERTIME;
      pmessage(0, MALL, inl_from, "---------- Overtime ----------");
      fprintf(inl_log, "---------- Overtime ---------\n");
      obliterate(0,KPROVIDENCE);

      inl_game.idx = 0;
      inl_game.end += inl_stat.overtime;
      /*  inl_game.counts[0] = inl_stat.overtime / (PERMIN*2); */
      inl_game.message = "%i %s left in overtime";

      announce_scores(0, MALL, inl_log);
    }
  else if (inl_game.end <= inl_stat.game_ticks)
    {
      pmessage(0, MALL, inl_from,
	       "------ Game ran out of time without a winner ------");

      fprintf(inl_log, "---------- Game Over (TIED) ---------\n");

      announce_scores(0, MALL, inl_log);
      game_over = 1;
    }

  /* run the stats script for an ending game */
  if (game_over) {
    FILE *fp;
    char pipe[256];
    char name[64];
    struct timeval tv;
    int c, official = 0;

    /* Set the queues so players go to default side */
    queues[QU_HOME].tournmask = HOMETEAM;
    queues[QU_AWAY].tournmask = AWAYTEAM;
    queues[QU_HOME_OBS].tournmask = HOMETEAM;
    queues[QU_AWAY_OBS].tournmask = AWAYTEAM;

    /* make every planet visible */
    for (c = 0; c < MAXPLANETS; c++)
      {
        planets[c].pl_info = ALLTEAM;
      }

    /* and make everybody re-join */
    obliterate(1, TOURNEND);

    status->tourn = 0;

    /* Tourn off t-mode */
    status->gameup |= (GU_CHAOS | GU_PRACTICE);
    status->gameup &= ~(GU_PAUSED);

    gettimeofday(&tv, (struct timezone *) 0);
    fprintf(inl_log, "TIME: Game ending at %d seconds\n", tv.tv_sec);
    fclose(inl_log);

    sleep(2); /* a kluge to allow time for all the ntservs to run */
              /* savestats() before stats-post processing I hope  */

    /* change all player offset position in playerfile to -1 so that
       stats don't get saved to next game's playerfile.  this is also
       done in reset_inl(1), but the delay causes synchronization
       problems so we do it again here.  this is ugly and should
       probably be fixed.
       CAVEAT: player must logout and login to play a new game.  -da */

    for (c=0; c<MAXPLAYER; c++)
      players[c].p_pos = -1;

    
    sprintf(name, "%s.%d", N_INLLOG, tv.tv_sec);
    rename(N_INLLOG, name);

    if ((inl_log = fopen(N_INLLOG,"w+"))==NULL) {
      ERROR(1,("Could not re-open INL log file.\n"));
      exit(1);
    }

    sprintf(name, "%s.%d", N_PLAYERFILE, tv.tv_sec);
    rename(N_PLAYERFILE, name);

    sprintf(name, "%s.%d", N_PLFILE, tv.tv_sec);
    rename(N_PLFILE, name);

    sprintf(name, "%s.%d", N_GLOBAL, tv.tv_sec);
    rename(N_GLOBAL, name);

    /* Stop cambot. */
    if (cambot_pid > 0) {
        kill(cambot_pid, SIGTERM);
        waitpid(cambot_pid, NULL, 0);
        cambot_pid = 0;
        sprintf(name, "%s.%d", Cambot_out, tv.tv_sec);
        rename(Cambot_out, name);
    }

    for (c=0; c < INLTEAM; c++) {
      if (inl_teams[c].flags & T_REGISTER) official++;
    }

    if (official == 2) {
      for (c=0; c < INLTEAM; c++) {
	int who = inl_teams[c].captain;
	pmessage(who, MINDIV, addr_mess(who, MINDIV),
		 "Official registration script starting.");
      }
      sprintf(pipe, "./end_tourney.pl -register %d", tv.tv_sec);
    } else {
      sprintf(pipe, "./end_tourney.pl -practice %d", tv.tv_sec);
    }

    fp = popen(pipe, "r");
    if (fp == NULL) {
      perror ("popen");
      pmessage(0, MALL|MCONQ, inl_from, "Stats: Cannot popen() %s", pipe);
    } else {
      char buffer[128], *line;

      line = fgets (buffer, 128, fp);
      while (line != NULL) {
	line[strlen(line)-1] = '\0';
	lmessage(line);
	line = fgets (buffer, 128, fp);
      }
      pclose (fp);
      if (official == 2) {
	for (c=0; c < INLTEAM; c++) {
	  int who = inl_teams[c].captain;
	  pmessage(who, MINDIV, addr_mess(who, MINDIV),
		   "Official registration script finished.");
	}
      }
    }

    reset_inl(1);

  } /* if (game_over) */

}

void
reset_inl(int is_end_tourney)
     /* is_end_tourney: boolean, used so that the galaxy isn't reset
	at the end of a tournament. */
{
  int c;

  /* Flushing messages */
  checkmess();

  /* Tourn off t-mode */
  status->gameup |= (GU_CHAOS | GU_PRACTICE);
  status->gameup &= ~(GU_PAUSED);

  /* obliterate is taken care of by the calling function for end_tourney */
  if (!is_end_tourney)
      obliterate(1, KPROVIDENCE);
  
  inl_stat.ticks = 0;

  for (c=0; c < INLTEAM; c++) {
    inl_teams[c].flags = 0;		/* reset all flags */
  }

  inl_stat.start_armies = 17;
  inl_stat.change = 1;
  inl_stat.flags = S_PREGAME;
  inl_stat.ticks = 0;
  inl_stat.game_ticks = 0;
  inl_stat.tmout_ticks = 0;
  inl_stat.time = INL_REGULATION * PERMIN;
  inl_stat.overtime = INL_OVERTIME * PERMIN;
  inl_stat.score_mode = 0;
  inl_stat.weighted_divisor = 0.0;

  inl_teams[HOME].team = HOME;
  inl_teams[HOME].name = "home";
  inl_teams[HOME].t_name = "Home";
  inl_teams[HOME].captain = NONE;
  inl_teams[HOME].side = HOMETEAM;
  inl_teams[HOME].side_index = NOT_CHOOSEN;
  inl_teams[HOME].start_armies = 17;
  inl_teams[HOME].time = INL_REGULATION;
  inl_teams[HOME].overtime = INL_OVERTIME;
  inl_teams[HOME].flags = 0;
  inl_teams[HOME].tmout = 1;
  inl_teams[HOME].planets = 10;
  inl_teams[HOME].score_mode = 0;
  inl_teams[HOME].abs_score = 0;
  inl_teams[HOME].semi_score = 0;
  inl_teams[HOME].weighted_score = 0.0;

  inl_teams[AWAY].team = AWAY;
  inl_teams[AWAY].name = "away";
  inl_teams[AWAY].t_name = "Away";
  inl_teams[AWAY].captain = NONE;
  inl_teams[AWAY].side = AWAYTEAM;
  inl_teams[AWAY].side_index = NOT_CHOOSEN;
  inl_teams[AWAY].start_armies = 17;
  inl_teams[AWAY].time = INL_REGULATION;
  inl_teams[AWAY].overtime = INL_OVERTIME;
  inl_teams[AWAY].flags = 0;
  inl_teams[AWAY].tmout = 1;
  inl_teams[AWAY].planets = 10;
  inl_teams[AWAY].score_mode = 0;
  inl_teams[AWAY].abs_score = 0;
  inl_teams[AWAY].semi_score = 0;
  inl_teams[AWAY].weighted_score = 0.0;

  /* Set player queues back to standard starting teams */
  if (is_end_tourney) {
    queues[QU_HOME].tournmask = FED;
    queues[QU_AWAY].tournmask = ROM;
    queues[QU_HOME_OBS].tournmask = FED;
    queues[QU_AWAY_OBS].tournmask = ROM;

    /* change all player offset position in playerfile to -1 so that
       stats don't get saved to next game's playerfile.
       CAVEAT: player must logout and login to play a new game.  -da */

    for (c=0; c<MAXPLAYER; c++)
      players[c].p_pos = -1;

    
  }

  if (!is_end_tourney) {
    doResources(1);
  }

}

init_server()
{
  register	  i;
  register struct planet *j;

  /* Tell other processes a game robot is running */
  status->gameup |= GU_INROBOT;

#ifdef nodef
  /* Fix planets */
  oldplanets = (struct planet *) malloc(sizeof(struct planet) * MAXPLANETS);
  MCOPY(planets, oldplanets, sizeof(struct planet) * MAXPLANETS);
#endif nodef

  /* Dont change the pickup queues around - just close them. */
#ifdef nodef
  /* Split the Waitqueues */
  /* Change the regular port to an observer port */
  queues[QU_PICKUP].tournmask	  = (HOMETEAM | AWAYTEAM);
#endif

  /* Nah, just close them down.. */
  queues[QU_PICKUP].q_flags	  &= ~(QU_OPEN);
  queues[QU_PICKUP_OBS].q_flags	  &= ~(QU_OPEN);

  /* these already set in queue.c - assume they are correct */
#ifdef nodef
  /* Ensure some inl queue initialization */
  queues[QU_HOME].max_slots  = TEAMSIZE;
  queues[QU_HOME].tournmask  = HOMETEAM;
  queues[QU_AWAY].max_slots  = TEAMSIZE;
  queues[QU_AWAY].tournmask  = AWAYTEAM;
  queues[QU_HOME_OBS].max_slots	 = TESTERS;
  queues[QU_HOME_OBS].tournmask	 = HOMETEAM;
  queues[QU_HOME_OBS].low_slot	 = MAXPLAYER-TESTERS;
  queues[QU_AWAY_OBS].max_slots	 = TESTERS;
  queues[QU_AWAY_OBS].tournmask	 = AWAYTEAM;
  queues[QU_AWAY_OBS].low_slot	 = MAXPLAYER-TESTERS;
#endif

  /* Open INL ports */
  queues[QU_HOME].free_slots = queues[QU_HOME].max_slots;
  queues[QU_AWAY].free_slots = queues[QU_AWAY].max_slots;
  queues[QU_HOME_OBS].free_slots = queues[QU_HOME_OBS].max_slots;
  queues[QU_AWAY_OBS].free_slots = queues[QU_AWAY_OBS].max_slots;

  status->tourn = 0;

  obliterate(1, KPROVIDENCE);

  /* If players are in game select teams alternately */
  for (i = 0; i<(MAXPLAYER-TESTERS); i++) {
    /* Let's deactivate the pickup slots and move pickup observers
       to the INL observer slots */
#ifdef nodef
    /* Don't change any pickup observers */
    if (players[i].p_status == POBSERV) continue;
#endif
    if (players[i].p_status != PFREE)
      {
	queues[players[i].w_queue].free_slots++;

	if (i%2)
	  players[i].w_queue = (players[i].p_status == POBSERV)
	    ? QU_HOME_OBS : QU_HOME;
	else
	  players[i].w_queue = (players[i].p_status == POBSERV)
	    ? QU_AWAY_OBS : QU_AWAY;

	queues[players[i].w_queue].free_slots--;
      }
  }

  /* dont set all the info - just what is neccessary */
  queues[QU_PICKUP].free_slots = 0;
  queues[QU_PICKUP_OBS].free_slots = 0;
  queues[QU_HOME].q_flags   |= QU_OPEN;
  queues[QU_AWAY].q_flags   |= QU_OPEN;
  queues[QU_HOME_OBS].q_flags	|= QU_OPEN;
  queues[QU_AWAY_OBS].q_flags	|= QU_OPEN;
#ifdef nodef
  queues[QU_PICKUP].low_slot = MAXPLAYER-TESTERS;
  queues[QU_PICKUP].max_slots = TESTERS;
  queues[QU_PICKUP].high_slot = MAXPLAYER;
#endif
}


void
cleanup()
{
  register struct player *j;
  register int i;

  SIGNAL(SIGALRM, SIG_IGN);

  status->gameup &= ~(GU_CHAOS | GU_PRACTICE);
  status->gameup &= ~(GU_PAUSED);

#ifdef nodef
  /* restore galaxy */
  MCOPY(oldplanets, planets, sizeof(struct planet) * MAXPLANETS);
#endif nodef

  /* Dont mess with the queue information - it is set correctly in queue.c */
#ifdef nodef
  /* restore old wait queue */
  queues[QU_PICKUP].tournmask=ALLTEAM;
#endif

  /* Open Pickup ports */
  queues[QU_PICKUP].free_slots = queues[QU_PICKUP].max_slots;
  queues[QU_PICKUP_OBS].free_slots = queues[QU_PICKUP_OBS].max_slots;

  /* Close INL ports */
  queues[QU_HOME].q_flags &= ~(QU_OPEN);
  queues[QU_AWAY].q_flags &= ~(QU_OPEN);
  queues[QU_HOME_OBS].q_flags &= ~(QU_OPEN);
  queues[QU_AWAY_OBS].q_flags &= ~(QU_OPEN);
  /* Set player queues back to standard starting teams */
  queues[QU_HOME_OBS].tournmask = HOMETEAM;
  queues[QU_AWAY_OBS].tournmask = AWAYTEAM;

  for (i = 0, j = &players[i]; i < MAXPLAYER; i++)
    {
      j = &players[i];
      if (j->p_status != PFREE)
	{
	  queues[j->w_queue].free_slots++;
	  if (players[i].p_status == POBSERV)
	    j->w_queue = QU_PICKUP_OBS;
	  else
	    j->w_queue = QU_PICKUP;
	  queues[j->w_queue].free_slots--;
	}
      if (j->p_status != PALIVE) continue;
      getship(&(j->p_ship), j->p_ship.s_type);
    }

  queues[QU_HOME].free_slots=0;
  queues[QU_AWAY].free_slots=0;
  queues[QU_HOME_OBS].free_slots=0;
  queues[QU_AWAY_OBS].free_slots=0;

  queues[QU_PICKUP].q_flags |= QU_OPEN;
  queues[QU_PICKUP_OBS].q_flags |= QU_OPEN;

#ifdef nodef
  queues[QU_PICKUP].free_slots += MAXPLAYER-TESTERS;
  queues[QU_PICKUP].low_slot = 0;
  queues[QU_PICKUP].high_slot = MAXPLAYER-TESTERS;
  queues[QU_PICKUP].max_slots = MAXPLAYER-TESTERS;
#endif

  /* Inform other processes that a game robot is no longer running */
  status->gameup &= ~(GU_INROBOT);

  pmessage(0, MALL, inl_from, "##########################");
  pmessage(0, MALL, inl_from, "#  The inl robot has left");
  pmessage(0, MALL, inl_from, "#  INL game is now over. ");
  pmessage(0, MALL, inl_from, "##########################");

  /* Flushing messages */
  checkmess();
  fclose(inl_log);
  exit(0);
}

start_countdown()
{
  int c;

  inl_stat.change = 0;

  inl_teams[HOME].side = sides[inl_teams[HOME].side_index].flag;
  inl_teams[AWAY].side = sides[inl_teams[AWAY].side_index].flag;

  /* Set the queues so players go to correct side */
  queues[QU_HOME].tournmask = inl_teams[HOME].side;
  queues[QU_AWAY].tournmask = inl_teams[AWAY].side;
  queues[QU_HOME_OBS].tournmask = inl_teams[HOME].side;
  queues[QU_AWAY_OBS].tournmask = inl_teams[AWAY].side;

  /* Nah.. just deactivate pickup queus.. */
#ifdef nodef
  queues[QU_PICKUP].tournmask = inl_teams[HOME].side | inl_teams[AWAY].side;
  queues[QU_PICKUP_OBS].tournmask =
    inl_teams[HOME].side | inl_teams[AWAY].side;
#endif

  obliterate(2, KPROVIDENCE);

  pmessage(0, MALL, inl_from, "Home team is   %s (%s): %s",
	   inl_teams[HOME].t_name,
	   inl_teams[HOME].name,
	   sides[inl_teams[HOME].side_index].name);
  pmessage(0, MALL, inl_from, "Away team is   %s (%s): %s",
	   inl_teams[AWAY].t_name,
	   inl_teams[AWAY].name,
	   sides[inl_teams[AWAY].side_index].name);
  pmessage(0, MALL, inl_from, " ");
  pmessage(0, MALL, inl_from, "Teams chosen.  Game will start in 1 minute.");
  pmessage(0, MALL, inl_from,
	   "----------- Game will start in 1 minute -------------");
  pmessage(0, MALL, inl_from,
	   "----------- Game will start in 1 minute -------------");

  inl_stat.flags = S_COUNTDOWN;
  inl_countdown.idx = 0;
  inl_countdown.end = inl_stat.ticks+INLSTARTFUSE;
  inl_countdown.action = start_tourney;
  inl_countdown.message = "Game start in %i %s";

}


start_tourney()
{
  int c;
  struct player* j;
  struct timeval tv;
  struct tm *tp;
  char *ap;
  char tmp[MSG_LEN];

  pmessage(0, MALL, inl_from, " ");

  pmessage(0, MALL, inl_from,
	   "INL Tournament starting now. %s(%s) vs. %s(%s).",
	   inl_teams[HOME].t_name,
	   inl_teams[HOME].name,
	   inl_teams[AWAY].t_name,
	   inl_teams[AWAY].name);

  pmessage(0, MALL, inl_from, " ");

  gettimeofday (&tv, (struct timezone *) 0);

  fprintf(inl_log, "TIME: Game started at %d seconds\n", tv.tv_sec);

  tp = localtime (&tv.tv_sec);
  ap = asctime (tp);

  strncpy (tmp, ap + 4, 15);
  *(tmp + 15) = '\0';
  pmessage (0, MALL, inl_from, "%s is the starting time of t-mode play.",
	    tmp);

  switch(inl_stat.score_mode) {
    case 0:
      pmessage(0, MALL, inl_from, "Planet scoring enabled.");
      break;
    case 1:
      pmessage(0, MALL, inl_from, "Continuous scoring enabled.");
      break;
    case 2:
      pmessage(0, MALL, inl_from, "Momentum scoring enabled.");
      break;
  }

  doResources(0);

#ifdef nodef
  inl_planets = (struct planet *) malloc(sizeof(struct planet) * MAXPLANETS);
  /*	MCOPY(planets, oldplanets, sizeof(struct planet) * MAXPLANETS); */
  MCOPY(planets, inl_planets, sizeof(struct planet) * MAXPLANETS);
#endif nodef

  inl_stat.flags |= S_TOURNEY;
  inl_stat.flags &= ~(S_PREGAME | S_COUNTDOWN | S_CONFINE);

  obliterate(1,TOURNSTART);
  reset_stats();

#ifdef nodef
  inl_stat.time = inl_teams[0].time * PERMIN;
  inl_stat.overtime = inl_teams[0].overtime * PERMIN;
#endif
  inl_stat.game_ticks = 0;

  status->gameup &= ~(GU_CHAOS | GU_PRACTICE);
#ifdef DEBUG
  printf("Setting tourn\n");
#endif
  status->tourn = 1;
  inl_game.idx=0;
  inl_game.end= inl_stat.time;
  /*	inl_game.counts[0] = inl_stat.time / ( PERMIN * 2); */
  inl_game.message = "%i %s left in regulation time";

  /* Start cambot. */
  if (inl_record) {
      int pid;
      pid = fork();
      if (pid < 0)
          perror("fork cambot");
      else if (pid == 0) {
          execl(Cambot, "cambot", 0);
          perror("execl cambot");
      }
      else {
          cambot_pid = pid;
      }
  }
}


obliterate(wflag, kreason)
     int wflag;
     char kreason;
{
  /* 0 = do nothing to war status, 1= make war with all, 2= make peace with all */
  struct player *j;
  int i, k;

  /* clear torps and plasmas out */
  MZERO(torps, sizeof(struct torp) * MAXPLAYER * (MAXTORP + MAXPLASMA));
  for (j = firstPlayer; j<=lastPlayer; j++) {
    if (j->p_status == PFREE)
      continue;
    if (j->p_status == POBSERV) {
      j->p_status = PEXPLODE;
      j->p_whydead = kreason;
      continue;
    }

    /* sanity checking.  eject players with negative player offset
       positions.  this can happen if there are back-to-back games
       without a server reset.  -da */

    if (!(j->p_flags & PFROBOT) && (j->p_pos < 0)) {

      pmessage(0, MALL, inl_from,
        "** Player %d ejected, must re-login to play.", j->p_no);
      pmessage(j->p_no, MINDIV | MCONQ, addr_mess(j->p_no, MINDIV),
        "You have been ejected due to player DB inconsistency.");
      pmessage(j->p_no, MINDIV | MCONQ, addr_mess(j->p_no, MINDIV),
        "This probably happened because of back-to-back games.");
      pmessage(j->p_no, MINDIV | MCONQ, addr_mess(j->p_no, MINDIV),
               "You must re-login to play.");

      j->p_status = PEXPLODE;
      j->p_whydead = KQUIT;
      j->p_explode = 10;
      j->p_whodead = 0;

      continue;

    }

    j->p_status = PEXPLODE;
    j->p_whydead = kreason;
    if (j->p_ship.s_type == STARBASE)
      j->p_explode = 2 * SBEXPVIEWS ;
    else
      j->p_explode = 10 ;
    j->p_ntorp = 0;
    j->p_nplasmatorp = 0;
    if (wflag == 1)
      j->p_hostile = (FED | ROM | ORI | KLI);	      /* angry */
    else if (wflag == 2)
      j->p_hostile = 0;	      /* otherwise make all peaceful */
    j->p_war = (j->p_swar | j->p_hostile);
    /* all armies in flight to be dropped on home (or alternate) planet */
    if (j->p_armies > 0) {
      k = 10 * (remap[j->p_team] - 1);
      if (k >= 0 && k <= 30) {
	for (i = 0; i < 10; i++) {
	  struct planet *pl = &planets[i+k];
	  if (pl->pl_owner == j->p_team) {
	    if (status->tourn) {
	      char addr_str[9] = "WRN->\0\0\0";
	      strncpy(&addr_str[5], j->p_mapchars, 3);
	      pmessage(0, 0, addr_str,
		       "ARMYTRACK %s {%s} (%d) beamed down %d armies at %s (%d) [%s]",
		       j->p_mapchars, shiptypes[j->p_ship.s_type],
		       j->p_armies, j->p_armies, 
		       pl->pl_name, pl->pl_armies, teamshort[pl->pl_owner]);
	      pl->pl_armies += j->p_armies;
	      j->p_armies = 0;
	    }
	    break;
	  }
	}
      }
    }
  }
}


countdown(counter,cnt)
     int counter;
     Inl_countdown *cnt;
{
  int i = 0;
  int j = 0;
  char *ms;

  if (cnt->end - cnt->counts[cnt->idx]*cnt->unit > counter)
    return;

  while ((cnt->idx != cnt->act) &&
	 (cnt->end - cnt->counts[cnt->idx+1]*cnt->unit <= counter))
    {
      cnt->idx++;
    }

  i = cnt->counts[cnt->idx];

  if (i > 60)
    {
      i = i / 60;
      j = 2;
    }
  if (i == 1)
    j++;

  pmessage(0, MALL, inl_from, cnt->message, i, time_msg[j]);
  if (cnt->idx++ == cnt->act) (*(cnt->action))();

}

doResources(startup)
     int startup;
{
  int i, j, k, which;

  /* this is all over the fucking place :-) */
  for (i = 0; i <= MAXTEAM; i++)
    {
      teams[i].s_turns = 0;
    }
  if (startup)
    {
      MCOPY (pdata, planets, sizeof (pdata));

      for (i = 0; i < MAXPLANETS; i++)
	{
	  planets[i].pl_info = ALLTEAM;
	}

      for (i = 0; i < 4; i++)
	{
	  /* one core AGRI */
	  planets[core_planets[i][random () % 4]].pl_flags |= PLAGRI;

	  /* one front AGRI */
	  which = random () % 2;
	  if (which)
	    {
	      j = random () % 2;
	      planets[front_planets[i][j]].pl_flags |= PLAGRI;

	      /* give fuel to planet next to agri (hde) */
	      planets[front_planets[i][!j]].pl_flags |= PLFUEL;

	      /* place one repair on the other front */
	      planets[front_planets[i][(random () % 3) + 2]].pl_flags |= PLREPAIR;

	      /* place 2 FUEL on the other front */
	      for (j = 0; j < 2; j++)
		{
		  do
		    {
		      k = random () % 3;
		    }
		  while (planets[front_planets[i][k + 2]].pl_flags & PLFUEL);
		  planets[front_planets[i][k + 2]].pl_flags |= PLFUEL;
		}
	    }
	  else
	    {
	      j = random () % 2;
	      planets[front_planets[i][j + 3]].pl_flags |= PLAGRI;
	      /* give fuel to planet next to agri (hde) */
	      planets[front_planets[i][(!j) + 3]].pl_flags |= PLFUEL;

	      /* place one repair on the other front */
	      planets[front_planets[i][random () % 3]].pl_flags |= PLREPAIR;

	      /* place 2 FUEL on the other front */
	      for (j = 0; j < 2; j++)
		{
		  do
		    {
		      k = random () % 3;
		    }
		  while (planets[front_planets[i][k]].pl_flags & PLFUEL);
		  planets[front_planets[i][k]].pl_flags |= PLFUEL;
		}
	    }

	  /* drop one more repair in the core
	     (home + 1 front + 1 core = 3 Repair) */

	  planets[core_planets[i][random () % 4]].pl_flags |= PLREPAIR;

	  /* now we need to put down 2 fuel (home + 2 front + 2 = 5 fuel) */

	  for (j = 0; j < 2; j++)
	    {
	      do
		{
		  k = random () % 4;
		}
	      while (planets[core_planets[i][k]].pl_flags & PLFUEL);
	      planets[core_planets[i][k]].pl_flags |= PLFUEL;
	    }
	}
    }
  else
    {
      for (i = 0; i < MAXPLANETS; i++)
	{
	  planets[i].pl_info = pdata[i].pl_info;
	  planets[i].pl_owner = pdata[i].pl_owner;
	  planets[i].pl_armies = inl_stat.start_armies;
	}
    }
}

void reset_stats()
{
  int i;
  struct player *j;

  /* Reset global stats. */
  /* Taken from main() in daemonII.c */
  status->time = 10;
  status->timeprod = 10;
  status->planets = 10;
  status->armsbomb = 10;
  status->kills = 10;
  status->losses = 10;

  /* Reset player stats. */
  for (i = 0; i < MAXPLAYER; i++) {
    j = &players[i];

    /* initial player state as given in getname */
    MZERO(&(j->p_stats), sizeof(struct stats));
#ifdef LTD_STATS
    ltd_reset(j);
#else
    j->p_stats.st_tticks = 1;
#endif
    j->p_stats.st_flags=ST_INITIAL;

    /* reset stats which are not in stats */
    j->p_kills = 0;
    j->p_armies = 0;
    j->p_genoplanets = 0;
    j->p_genoarmsbomb = 0;
    j->p_planets = 0;
    j->p_armsbomb = 0;
  }
}
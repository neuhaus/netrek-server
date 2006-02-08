/* pret.c
*/

/*
 * Pre T entertainment by Nick Slager
 * Based on Newbie Server robot by Jeff Nowakowski
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
#include "pretdefs.h"

int debug=0;

char *roboname = "Kathy";

static char *teamNames[9] = {" ", "Federation", "Romulans", " ", "Klingons",
                              " ", " ", " ", "Orions"};

#define NUMADJ 12
static char    *adj_s[NUMADJ] = {
    "VICIOUS", "RUTHLESS", "IRONFISTED", "RELENTLESS",
    "MERCILESS", "UNFLINCHING", "FEARLESS", "BLOODTHIRSTY",
    "FURIOUS", "DESPERATE", "FRENZIED", "RABID"};

#define NUMNAMES        20

static char    *names[NUMNAMES] =
{"Annihilator", "Banisher", "Blaster",
 "Demolisher", "Destroyer", "Eliminator",
 "Eradicator", "Exiler", "Obliterator",
 "Razer", "Demoralizer", "Smasher",
 "Shredder", "Vanquisher", "Wrecker",
 "Ravager", "Despoiler", "Abolisher",
 "Emasculator", "Decimator"};

/*
 * #define NUMNAMES     3 static char *names[NUMNAMES] = { "guest", "guest",
 * "guest" };
 */

static  char    hostname[64];

int target;  /* Terminator's target 7/27/91 TC */
int phrange; /* phaser range 7/31/91 TC */
int trrange; /* tractor range 8/2/91 TC */
int ticks = 0;
int oldmctl;
static realT = 0;
int pt_robots = 0;
int team1=0;
int team2=0;
static int debugTarget = -1;
static int debugLevel = 0;

static void cleanup(int);
void checkmess(int);
static void start_internal(char *type);
static void obliterate(int wflag, char kreason, int killRobots);
static void start_a_robot(char *team);
static void stop_a_robot(void);
static int is_robots_only(void);
static void exitRobot(void);
static char * namearg(void);
static int num_players(int *next_team);
static int rprog(char *login, char *monitor);
static void stop_this_bot(struct player * p);
static void save_armies(struct player *p);
static void resetPlanets(void);
static void checkPreTVictory();
static int num_humans(int team);
static int totalPlayers();
static void checkMessage(struct message *p);
 
static void
reaper(int sig)
{
    int stat=0;
    static int pid;

    while ((pid = WAIT3(&stat, WNOHANG, 0)) > 0)
      ;
        pt_robots--;
    HANDLE_SIG(SIGCHLD,reaper);
}

#ifdef PRETSERVER
int
main(argc, argv)
     int             argc;
     char           *argv[];
{
    int team = 4;
    int pno;
    int class;                  /* ship class 8/9/91 TC */
    int i;
 
#ifndef TREKSERVER
    if (gethostname(hostname, 64) != 0) {
        perror("gethostname");
        exit(1);
    }
#else
    strcpy(hostname, TREKSERVER);
#endif

    srandom(time(NULL));

    getpath();
    (void) SIGNAL(SIGCHLD, reaper);
    openmem(1);
    strcpy(robot_host,REMOTEHOST);
    readsysdefaults();
    SIGNAL(SIGALRM, checkmess);             /*the def signal is needed - MK */
    if (!debug)
        SIGNAL(SIGINT, cleanup);

    class = STARBASE;
    target = -1;                /* no target 7/27/91 TC */
    if ( (pno = pickslot(QU_PRET_DMN)) < 0) {
       printf("exiting! %d\n", pno);
       exit(0);
    }
    me = &players[pno];
    myship = &me->p_ship;
    mystats = &me->p_stats;
    lastm = mctl->mc_current;
    /* At this point we have memory set up.  If we aren't a fleet, we don't
       want to replace any other robots on this team, so we'll check the
       other players and get out if there are any on our team.
       */

    robonameset(me); /* set the robot@nowhere fields */
    enter(team, 0, pno, class, "Kathy"); /* was BATTLESHIP 8/9/91 TC */

    me->p_pos = -1;                              /* So robot stats don't get saved */
    me->p_flags |= (PFROBOT | PFCLOAK);          /* Mark as a robot and hide it */
    me->p_x = -100000;                           /* don't appear in the galaxy */
    me->p_y = -100000;
    me->p_hostile = 0;
    me->p_swar = 0;
    me->p_war = 0;
    me->p_team = 0;     /* indep */

    oldmctl = mctl->mc_current;
#ifdef nodef
    for (i = 0; i <= oldmctl; i++) {
        check_command(&messages[i]);
    }
#endif

    status->gameup |= GU_PRET;
    queues[QU_PRET_PLR].q_flags |= QU_REPORT;
    queues[QU_PRET_OBS].q_flags |= QU_REPORT;
    queues[QU_PICKUP].q_flags ^= QU_REPORT;
    queues[QU_PICKUP_OBS].q_flags ^= QU_REPORT;

    /* Robot is signalled by the Daemon */
    ERROR(3,("\nRobot Using Daemon Synchronization Timing\n"));
   
    me->p_process = getpid();
    me->p_timerdelay = HOWOFTEN; 

    /* allows robots to be forked by the daemon -- Evil ultrix bullshit */
    SIGSETMASK(0);

    /* set up team1 and team2 so we're not always playing rom/fed */
    team1 = (random()%2) ? FED : KLI;
    team2 = (random()%2) ? ROM : ORI;

    me->p_status = PALIVE;              /* Put robot in game */
    resetPlanets();

    /* Only allow Rom/Fed game to make robot team selection easier. */
    /* Disable this because it breaks on timercide.  The other team
       needs to come in as a 3rd race after being timercided.
    queues[QU_PICKUP].tournmask = FED|ROM;
    */

    while (1) {
        PAUSE(SIGALRM);
    }
}

void checkmess(int unused)
{ 
    int         shmemKey = PKEY;
    int i;
    static int no_humans = 0;
    static int no_bots = 0;
    static int time_in_T = 0;

    HANDLE_SIG(SIGALRM,checkmess);
    me->p_ghostbuster = 0;         /* keep ghostbuster away */
    if (me->p_status != PALIVE){  /*So I'm not alive now...*/
        ERROR(2,("ERROR: Kathy died??\n"));
        cleanup(0);   /*Kathy is dead for some unpredicted reason like xsg */
    }

    /* make sure shared memory is still valid */
    if (shmget(shmemKey, SHMFLAG, 0) < 0) {
        exit(1);
        ERROR(2,("ERROR: Invalid shared memory\n"));
    }

    ticks++;

    /* End the current game if no humans for 60 seconds. */
    if ((ticks % ROBOCHECK) == 0) {
        if (no_humans >= 60)
            cleanup(0); /* Doesn't return. */

        if (is_robots_only())
            no_humans += ROBOCHECK / PERSEC;
        else
            no_humans = 0;
    }

    /* Check to see if we should start adding bots again */
    if ((ticks % ROBOCHECK) == 0) {
        if ((no_bots > time_in_T || no_bots >= 300) && realT) {
            messAll(255,roboname,"Pre-t Entertainment starting back up.");
            realT = 0;
        }

        if (num_humans(0) < 8 && realT) {
            if((no_bots % 60) == 0) {
                messAll(255,roboname,"Pre-T entertainment will start in %d minutes if T-mode doesn't return...", (((300<time_in_T)?300:time_in_T)-no_bots)/60);
            }
            no_bots += ROBOCHECK / PERSEC;
        }
        else
            no_bots = 0;
    }

    /* check if either side has won */
    if ((ticks % ROBOCHECK) == 0) {
        checkPreTVictory();
    }

    /* Stop or start a robot. */
    if ((ticks % ROBOCHECK) == 0) {
        int next_team;
        int np = num_players(&next_team);
 
        if (!(ticks % ROBOEXITWAIT))
		{
            if(debugTarget != -1) {
                messOne(255, roboname, debugTarget, "Total Players: %d  Current bots: %d  Current human players: %d", totalPlayers(), pt_robots, num_humans(0));
            }
            if(totalPlayers() > PT_MAX_WITH_ROBOTS) {
                if(debugTarget != -1) {
                    messOne(255, roboname, debugTarget, "Stopping a robot");
                    messOne(255, roboname, debugTarget, "Current bots: %d  Current human players: %d", pt_robots, num_humans(0));
                }
        		stop_a_robot();
            }
            else if (pt_robots < PT_ROBOTS && totalPlayers() < PT_MAX_WITH_ROBOTS  && realT == 0)
            {
                if(debugTarget != -1) {
                    messOne(255, roboname, debugTarget, "Starting a robot");
                    messOne(255, roboname, debugTarget, "Current bots: %d  Current human players: %d", pt_robots, num_humans(0));
                }
                if (next_team == FED)
                    start_a_robot("-Tf");
                else if (next_team == KLI)
                    start_a_robot("-Tk");
                else if (next_team == ORI)
                    start_a_robot("-To");
                else
                    start_a_robot("-Tr");
            }
        }
        if(pt_robots == 0 && totalPlayers() >= 8) {
            time_in_T += ROBOCHECK / PERSEC;
            if(realT == 0) {
                time_in_T = 0;
                realT = 1;
                status->gameup &= ~GU_BOT_IN_GAME;
                messAll(255,roboname,"Resetting for real T-mode!");
                obliterate(1,KPROVIDENCE, 0);
                resetPlanets();
            }
        }
    }

    if ((ticks % SENDINFO) == 0) {
        if (pt_robots > 0) {
            messAll(255,roboname,"Welcome to the Pre-T Entertainment.");
            messAll(255,roboname,"Your team wins if you're up by at least 3 planets.");
        }
    }
    while (oldmctl!=mctl->mc_current) {
        oldmctl++;
        if (oldmctl==MAXMESSAGE) oldmctl=0;
        if (messages[oldmctl].m_flags & MINDIV) {
            if (messages[oldmctl].m_recpt == me->p_no)
                checkMessage(&messages[oldmctl]);
        }
    }

}

static void checkMessage(struct message *mess) {
    int i, len;
    char *comm;
    char query[20];
    int num = 0;
    len = strlen(mess->m_data);
    if(len <= 8)
        return;

    for (i=8; i<len && isspace(mess->m_data[i]); ++i) continue;
    comm = strdup(&mess->m_data[i]);
    len = strlen(comm);

    /* Convert first word in msg to uppercase */
    for (i=0; (i < len && !isspace(comm[i])); i++)
    {
        comm[i] = toupper(comm[i]);
    }
    len = i;

    sscanf(comm, "%s %d", query, &num);
    if(strcmp(query, "DEBUG") == 0) {
        debugTarget = mess->m_from;
        if(num > 0) {
            debugLevel = num;
            messOne(255, roboname, mess->m_from, "Sending debug info to  %d", debugTarget);
        }
        else {
            debugTarget = -1;
            debugLevel = 0;
            messOne(255, roboname, mess->m_from, "Not sending debug info");
        }
    }
}

static int is_robots_only(void)
{
   return !num_humans(0);
}

static int totalPlayers() {
   int i;
   struct player *j;
   int count = 0;

   for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
        if (j->p_status == PFREE)
            continue;
        if (j->p_flags & PFROBOT)
            continue;
        if (j->p_status == POBSERV)
            continue;
        count++;
   }
   return count;
}

static int num_humans(int team) {
   int i;
   struct player *j;
   int count = 0;

   for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
        if (j->p_status == PFREE)
            continue;
        if (j->p_flags & PFROBOT)
            continue;
        if (j->p_status == POBSERV)
            continue;
        if(team != 0 && j->p_team != team)
            continue;
        if (!rprog(j->p_login, j->p_full_hostname)) {
          /* Found a human. */
            count++;
            if(debugTarget != -1 && debugLevel == 2) {
                messOne(255, roboname, debugTarget, "%d: Counting %s (%s %s) as a human", i, j->p_mapchars, j->p_login, j->p_full_hostname);
            }
        }
        else {
            if(debugTarget != -1 && debugLevel == 2) {
                messOne(255, roboname, debugTarget, "%d: NOT Counting %s (%s %s) as a human", i, j->p_mapchars, j->p_login, j->p_full_hostname);
            }
        }
   }

   return count;
}

static void stop_a_robot(void)
{
    int i;
    struct player *j;
    int teamToStop;

    if(debugTarget != -1 && debugLevel == 3) {
        messOne(255, roboname, debugTarget, "#1(%d): %d  #2(%d): %d", team1, num_humans(team1), team2, num_humans(team2));
    }
    if(num_humans(team1) < num_humans(team2))
        teamToStop = team1;
    else
        teamToStop = team2;

    if(debugTarget != -1 && debugLevel == 3) {
        messOne(255, roboname, debugTarget, "Stopping from %d", teamToStop);
    }
    /* Nuke robot from the team with the fewest humans. */
    for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
        if (j->p_status == PFREE)
            continue;
        if (j->p_flags & PFROBOT)
            continue;

        /* If he's at the MOTD we'll get him next time. */
        if (j->p_team == teamToStop && j->p_status == PALIVE && rprog(j->p_login, j->p_full_hostname)) {
            stop_this_bot(j);
            return;
        }
    }
}

/* this is by no means foolproof */

static int
rprog(char *login, char *robotHost)
{
    int             v;
    char localHostName[80];

    gethostname(localHostName, 80);

    if (strcmp(login, PRE_T_ROBOT_LOGIN) == 0 && (!strcmp(localHostName, robotHost) || !strcmp(robot_host, robotHost)))
        return 1;

    return 0;
}

static void stop_this_bot(struct player *p) {
    p->p_ship.s_type = STARBASE;
    p->p_whydead=KQUIT;
    p->p_explode=10;
    p->p_status=PEXPLODE;
    p->p_whodead=0;

    pmessage(0, MALL, "Kathy->ALL", 
        "Robot %s (%2s) was ejected to make room for a human player.",
        p->p_name, p->p_mapchars);
    if ((p->p_status != POBSERV) && (p->p_armies>0)) save_armies(p);
/*    pt_robots--;*/
}

static void save_armies(struct player *p)
{
  int i, k;

  k=10*(remap[p->p_team]-1);
  if (k>=0 && k<=30) for (i=0; i<10; i++) {
    if (planets[i+k].pl_owner==p->p_team) {
      planets[i+k].pl_armies += p->p_armies;
      pmessage(0, MALL, "Kathy->ALL", "%s's %d armies placed on %s",
                     p->p_name, p->p_armies, planets[k+i].pl_name);
      break;
    }
  }
}

static int
num_players(int *next_team)
{
    int i;
    struct player *j;
    int team_count[MAXTEAM+1];

    int c = 0;

    team_count[FED] = 0;
    team_count[KLI] = 0;
    team_count[ROM] = 0;
    team_count[ORI] = 0;

    for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
        if (j->p_status != PFREE && j->p_status != POBSERV &&
            !(j->p_flags & PFROBOT))
            {
                team_count[j->p_team]++;
                c++;
            }
    }

    /* team sanity check */
    if(team_count[FED] != team_count[KLI]) {
      int t = (team_count[FED]>team_count[KLI])?FED:KLI;
      if(team1 != t) {
        team1 = t;
        resetPlanets();
      }
    }
    if(team_count[ROM] != team_count[ORI]) {
      int t = (team_count[ROM]>team_count[ORI])?ROM:ORI;
      if(team2 != t) {
        team2 = t;
        resetPlanets();
      }
    }

    /* Assign which team gets the next robot. */
    if (team_count[team1] > team_count[team2])
        *next_team = team2;
    else
        *next_team = team1;

    return c;
}

static char           *
namearg(void)
{
    register        i, k = 0;
    register struct player *j;
    char           *name;
    int             namef = 1;

    while (1) {

        name = names[random() % NUMNAMES];
        k++;

        namef = 0;
        for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
            if (j->p_status != PFREE && strncmp(name, j->p_name, strlen(name) - 1)
                == 0) {
                namef = 1;
                break;
            }
        }
        if (!namef)
            return name;

        if (k == 50)
            return "guest";
    }
}


static void
start_a_robot(char *team)
{
    char            command[256];
    int pid;

    /* No  more remote shell crap...
    sprintf(command, "%s %s %s %s -h %s -p %d -n '%s' -X %s -b -O -i -I",
            RCMD, robot_host, OROBOT, team, hostname, PORT, namearg(), PRE_T_ROBOT_LOGIN );
            */
    /* robothost can be used to tell the robot where to connect to */
    sprintf(command, "%s %s -h %s -p %d -n '%s' -X %s -b -O -I",
            OROBOT, team, (strlen(robot_host))?robot_host:hostname, PORT, namearg(), PRE_T_ROBOT_LOGIN );
   
    pid = fork();
    if (pid == -1)
     return;
    if (pid == 0) {
        SIGNAL(SIGALRM, SIG_DFL);
        execl("/bin/sh", "sh", "-c", command, 0);
        perror("pret'execl");
        _exit(1);
    }
    pt_robots++;
    status->gameup |= GU_BOT_IN_GAME;
}

static void start_internal(char *type)
{
    char *argv[6];
    u_int argc = 0;

    argv[argc++] = "robot";
    if ((strncmp(type, "iggy", 2) == 0) || 
        (strncmp(type, "hunterkiller", 2) == 0)) {
        argv[argc++] = "-Ti";
        argv[argc++] = "-P";
        argv[argc++] = "-f";    /* Allow more than one */
    } else if (strncmp (type, "cloaker", 2) == 0) {
        argv[argc++] = "-Ti";
        argv[argc++] = "-C";    /* Never uncloak */
        argv[argc++] = "-F";    /* Needs no fuel */
        argv[argc++] = "-f";
    } else if (strncmp (type, "hoser", 2) == 0) {
        argv[argc++] = "-p";
        argv[argc++] = "-f";
    } else return;

    argv[argc] = NULL;
    if (fork() == 0) {
        SIGNAL(SIGALRM, SIG_DFL);
        execv(Robot,argv);
        perror(Robot);
        _exit(1);
    }
}

static void cleanup(int unused)
{
    register struct player *j;
    register int i, retry;

    do {
        /* terminate all robots */
        for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
            if ((j->p_status == PALIVE) && rprog(j->p_login, j->p_full_hostname))
                stop_this_bot(j);
        }

        USLEEP(2000000); 
        retry=0;
        for (i = 0, j = players; i < MAXPLAYER; i++, j++) {
            if ((j->p_status != PFREE) && rprog(j->p_login, j->p_full_hostname))
                retry++;
        }
    } while (retry);            /* Some robots havn't terminated yet */

    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
        if ((j->p_status != PALIVE) || (j == me)) continue;
        getship(&(j->p_ship), j->p_ship.s_type);
    }

    obliterate(1,KPROVIDENCE, 1);
    status->gameup &= ~GU_PRET;
    queues[QU_PRET_PLR].q_flags ^= QU_REPORT;
    queues[QU_PRET_OBS].q_flags ^= QU_REPORT;
    queues[QU_PICKUP].q_flags |= QU_REPORT;
    queues[QU_PICKUP_OBS].q_flags |= QU_REPORT;
    exitRobot();
}

/* a pre-t victory is when one team is up by 3 planets */
static void checkPreTVictory() {
    int f, r, k, o, i;
    int winner = -1;

    /* don't interfere with a real game */
    if(pt_robots == 0) return;

    f = r = k = o = 0;
    for(i=0;i<40;++i) {
       if(planets[i].pl_owner == FED) f++; 
       if(planets[i].pl_owner == ROM) r++; 
       if(planets[i].pl_owner == KLI) k++; 
       if(planets[i].pl_owner == ORI) o++; 
    }
    if(f>=13) winner = FED;
    if(r>=13) winner = ROM;
    if(k>=13) winner = KLI;
    if(o>=13) winner = ORI;

    if(winner > 0) {
        messAll(255,roboname,"The %s have won this round of pre-T entertainment!", teamNames[winner]);
        obliterate(1,KPROVIDENCE, 0);
        resetPlanets();
    }
}

static void resetPlanets(void) {
    int i;
    int owner;
    for(i=0;i<40;i++) {
        switch(i/10) {
            case 0:
                owner = FED;
                break;
            case 1:
                owner = ROM;
                break;
            case 2:
                owner = KLI;
                break;
            default:
                owner = ORI;
                break;
        }
        if(planets[i].pl_armies < 3) 
            planets[i].pl_armies += (random() % 3) + 2;
        if(planets[i].pl_armies > 7) 
            planets[i].pl_armies = 8 - (random() % 3);
        if(owner != team1 && owner  != team2)
            planets[i].pl_armies = 30;
        planets[i].pl_owner = owner;
    }
}

static void exitRobot(void)
{
    SIGNAL(SIGALRM, SIG_IGN);
    if (me != NULL && me->p_team != ALLTEAM) {
        if (target >= 0) {
            messAll(255,roboname, "I'll be back.");
        }
        else {
            messAll(255,roboname,"#");
            messAll(255,roboname,"#  Kathy is tired.  Pre-T Entertainment is over "
                    "for now");
            messAll(255,roboname,"#");
        }
    }

    freeslot(me);
    exit(0);
}


static void obliterate(int wflag, char kreason, int killRobots)
{
    /* 0 = do nothing to war status, 1= make war with all, 2= make peace with all */
    struct player *j;
    int i, k;

    /* clear torps and plasmas out */
    MZERO(torps, sizeof(struct torp) * MAXPLAYER * (MAXTORP + MAXPLASMA));
    for (j = firstPlayer; j<=lastPlayer; j++) {
        if (j->p_status == PFREE)
            continue;
        if ((j->p_flags & PFROBOT) && killRobots == 0)
            continue;
        j->p_status = PEXPLODE;
        j->p_whydead = kreason;
        if (j->p_ship.s_type == STARBASE)
            j->p_explode = 2 * SBEXPVIEWS ;
        else
            j->p_explode = 10 ;
        j->p_ntorp = 0;
        j->p_nplasmatorp = 0;
        if (wflag == 1)
            j->p_hostile = (FED | ROM | ORI | KLI);         /* angry */
        else if (wflag == 2)
            j->p_hostile = 0;       /* otherwise make all peaceful */
        j->p_war = (j->p_swar | j->p_hostile);
    }
}
#endif  /* PRETSERVER */


#ifndef PRETSERVER
int
main(argc, argv)
     int             argc;
     char           *argv[];
{
    printf("You don't have PRETSERVER option on.\n");
}

#endif /* PRETSERVER */
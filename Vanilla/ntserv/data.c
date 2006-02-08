/* $Id: data.c,v 1.3 2005/09/27 12:26:37 quozl Exp $
 */

#include "copyright.h"
#include "defs.h"
#include "struct.h"
#include "data.h"

#include <sys/types.h>		/* needed to define fd_set for inputMask */
#include INC_SYS_SELECT

struct player *players;
struct player *me;
struct torp *torps;
struct status *status;
struct ship *myship;
struct stats *mystats;
struct planet *planets;
struct phaser *phasers;
struct message *messages;
struct mctl *mctl;
struct team *teams;
struct ship *shipvals;
struct pqueue *queues;
struct queuewait *waiting;

int	oldalert = PFGREEN;	/* Avoid changing more than we have to */

#ifdef XSG
int     remap[16] = { 0, 1, 2, 0, 3, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0 };
#else
int 	remap[9] = { 0, 1, 2, -1, 3, -1, -1, -1, 4 };
#endif
int	reality = 10;		/* reality updates per second */
int	minskip = 10;		/* maximum client updates per second */
int	defskip = 5;		/* default client updates per second */
int	maxskip = 1;		/* minimum client updates per second */
int	messpend;
int	lastcount;
int	mdisplayed;
int	redrawall;
int	selfdest;
int	udcounter;
int	lastm;
int	delay;			/* delay for decaring war */
int	rdelay;			/* delay for refitting */
int	doosher = 0;		/* NBT 11-4-92 print doosh messages */
int	dooshignore = 0;	/* NBT 3/30/92 ignore doosh messages */
int	macroignore = 0;
int	whymess = 0;		/* NBT 6/1/93  improved why dead messages */
int	show_sysdef=0;		/* NBT 6/1/93  show sysdef on galactic */
int	max_pop=999;		/* NBT 9/9/93  sysdef for max planet pop */
int	mapmode = 1; 
int	namemode = 1; 
int	showStats;
int	showShields;
int	warncount = 0;
int	warntimer = -1;
int	infomapped = 0;
int	mustexit = 0;
int	messtime = MESSTIME;
int	keeppeace = 0;
int	showlocal = 2;
int 	showgalactic = 2;
char 	*shipnos="0123456789abcdefghijklmnopqrstuvwxyz";
int 	sock= -1;
int 	xtrekPort=27320;
int	shipPick;
int	teamPick;
int	repCount=0;
char 	namePick[NAME_LEN];
char 	passPick[NAME_LEN];
fd_set	inputMask;
int 	nextSocket;
char	*host;
int 	noressurect=0;
int	userVersion=0, userUdpVersion=0;
int	bypassed=0;
int	top_armies=30;		/*initial army count default */
int	errorlevel=1;		/* controlling amount of error info */
int	dead_warp=0;		/* use warp 14 for death detection */
int	surrenderStart=1;	/* # of planets to start surrender counter */
int	sbplanets=5;		/* # of planets to get a base */
int	restrict_bomb=1;        /* Disallow bombing outside of T-Mode */
int	no_unwarring_bombing=1; /* No 3rd space bombing */

char *shipnames[NUM_TYPES] = {
      "Scout", "Destroyer", "Cruiser", "Battleship", 
      "Assault", "Starbase", "SGalaxy", "AT&T"
};
char *shiptypes[NUM_TYPES] = {"SC", "DD", "CA", "BB", "AS", "SB", "GA", "AT"}; 
char *weapontypes[WP_MAX] = {"PLASMA", "TRACTOR"};
char *planettypes[MAXPLANETS] = {
    "00", "01", "02", "03", "04", "05", "06", "07", "08", "09",
    "10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
    "20", "21", "22", "23", "24", "25", "26", "27", "28", "29",
    "30", "31", "32", "33", "34", "35", "36", "37", "38", "39"
};


#ifdef RSA			/* NBT added */
char    RSA_client_type[256];   /* from LAB 4/15/93 */
char	testdata[KEY_SIZE];
int	RSA_Client;
#else
char	testdata[NAME_LEN];
#endif

#ifdef CHECKMESG		/* NBT added */
int     checkmessage = 0;
int     logall=0;
int     loggod=0;
int   eventlog=0;
#endif

int	start_robot = 0;
int	robot_pid = 0;

#ifdef FEATURES
char	*version;
#endif

#ifdef CHAIN_REACTION
int	why_dead=0;
#endif

#ifdef OBSERVERS
int	Observer=0;
#endif

int	SBhours=0;

int	testtime= -1;
int 	tournplayers=5;
int 	shipsallowed[NUM_TYPES];
int 	weaponsallowed[WP_MAX];
int 	plkills=2;
int 	sbrank=3;
int 	startplanets[MAXPLANETS];
int 	binconfirm=1;
int	chaosmode=0;
int	topgun=0; /* added 12/9/90 TC */
int	nodiag=1;
int	killer=1; /* added 1/28/93 NBT */
int	resetgalaxy=0; /* added 2/6/93 NBT */
int     newturn=0; /* added 1/20/91 TC */
int	planet_move=0;	/* added 9/28/92 NBT */
int	RSA_confirm=0;  /* added 10/18/92 NBT */
int	check_scum=0;	/* added 7/21/93 NBT */
int	wrap_galaxy=0; /* added 6/29/95 JRP */
int	pingpong_plasma=0; /* added 7/6/95 JRP */
int	max_chaos_bases=2; /* added 7/7/95 JRP */
int	errors;	/*fd to try and fix stderrs being sent over socket NBT 9/23/93*/
int	hourratio=1;	/* Fix thing to make guests advance fast */
int	minRank=0;         /* isae - Minimal rank allowed to play */
int	clue=0;		/* NBT - clue flag for time_access() */
int	cluerank=6;
int	clueVerified=0;
int     clueFuse=0;
int     clueCount=0;
char	*clueString;
int	hiddenenemy=1;
int	loadcheck=0;
float   maxload=0.7;
int	vectortorps = 0;
int	galactic_smooth = 0;

int	udpSock=(-1);     /* UDP - the almighty socket */
int	commMode=0;     /* UDP - initial mode is TCP only */
int	udpAllowed=1;   /* UDP - sysdefaults */

double	oldmax = 0.0;

extern double	Sin[], Cos[];		 /* Initialized in sintab.c */

char	teamlet[] = {'I', 'F', 'R', 'X', 'K', 'X', 'X', 'X', 'O'};
char	*teamshort[9] = {"IND", "FED", "ROM", "X", "KLI", "X", "X", "X", "ORI"};
char	login[PSEUDOSIZE];

struct rank ranks[NUMRANKS] = {
    { 0.0, 0.0, 0.0, "Ensign"},
    { 2.0, 1.0, 0.0, "Lieutenant"},

/* change: 12/8/90 TC no defense required    { 4.0, 2.0, 0.8, "Lt. Cmdr."}, 
    { 8.0, 3.0, 0.8, "Commander"},
    {15.0, 4.0, 0.8, "Captain"},
    {20.0, 5.0, 0.8, "Flt. Capt."},
    {25.0, 6.0, 0.8, "Commodore"},
    {30.0, 7.0, 0.8, "Rear Adm."},
    {40.0, 8.0, 0.8, "Admiral"}};*/

    { 4.0, 2.0, 0.0, "Lt. Cmdr."}, 
    { 8.0, 3.0, 0.0, "Commander"},
    {15.0, 4.0, 0.0, "Captain"},
    {20.0, 5.0, 0.0, "Flt. Capt."},
    {25.0, 6.0, 0.0, "Commodore"},
    {30.0, 7.0, 0.0, "Rear Adm."},
    {40.0, 8.0, 0.0, "Admiral"}};

int             startTkills, startTlosses, startTarms, startTplanets,
                startTticks;

int		startSBkills, startSBlosses, startSBticks;

#ifdef PING
int     ping=0;                 /* to ping or not to ping, client's decision*/
LONG    packets_sent=0;         /* # all packets sent to client */
LONG    packets_received=0;     /* # all packets received from client */
int     ping_ghostbust=0;       /* ghostbust timer */

/* defaults variables */
int     ping_freq=1;            /* ping freq. in seconds */
int     ping_iloss_interval=10; /* in values of ping_freq */
int     ping_allow_ghostbust=0; /* allow ghostbust detection from
                                   ping_ghostbust (cheating possible)*/
int     ping_ghostbust_interval=5; /* in ping_freq, when to ghostbust
                                      if allowed */
#endif

#ifdef SB_TRANSWARP
int           twarpMode=0;       /* isae - SB transwarp */
int           twarpSpeed=60;     /* isae - Warp speed */
#endif


int send_short = 0;
int send_threshold = 0;		/* infinity */
int actual_threshold = 0;	/* == send_threshold / numupdates */
int numupdates = 5;		/* For threshold */

char Global[FNAMESIZE];
char Scores[FNAMESIZE];
char PlFile[FNAMESIZE];
char Motd[FNAMESIZE] = N_MOTD ;
char Motd_Path[FNAMESIZE];
char Daemon[FNAMESIZE];
char Robot[FNAMESIZE];
char LogFileName[FNAMESIZE];
char PlayerFile[FNAMESIZE];
char PlayerIndexFile[FNAMESIZE];
char ConqFile[FNAMESIZE];
char SysDef_File[FNAMESIZE];
char Time_File[FNAMESIZE];
char Clue_Bypass[FNAMESIZE];
char Banned_File[FNAMESIZE];
char Scum_File[FNAMESIZE];
char Error_File[FNAMESIZE];
char Bypass_File[FNAMESIZE];
#ifdef RSA
char RSA_Key_File[FNAMESIZE];
#endif
#ifdef AUTOMOTD
char MakeMotd[FNAMESIZE];
#endif
#ifdef CHECKMESG
char MesgLog[FNAMESIZE];
char GodLog[FNAMESIZE];
#endif
#ifdef FEATURES
char Feature_File[FNAMESIZE];
#endif
#ifdef ONCHECK
char On_File[FNAMESIZE];
#endif
#ifdef BASEPRACTICE
char Basep[FNAMESIZE];
#endif
#ifdef NEWBIESERVER
char Newbie[FNAMESIZE];
#endif
#ifdef PRETSERVER
char PreT[FNAMESIZE];
#endif
#if defined(BASEPRACTICE) || defined(NEWBIESERVER)  || defined(PRETSERVER)
char Robodir[FNAMESIZE];
char robofile[FNAMESIZE];
char robot_host[FNAMESIZE];
#endif
#ifdef DOGFIGHT
char Mars[FNAMESIZE];
#endif
char Puck[FNAMESIZE];
char Inl[FNAMESIZE];
int inl_record = 0;

char Access_File[FNAMESIZE];
char NoCount_File[FNAMESIZE];
char Prog[FNAMESIZE];
char LogFile[FNAMESIZE];

#ifdef DOGFIGHT
/* Needed for Mars */
int   nummatch=4;
int   contestsize=4;
int     dogcounts=0;
#endif

char Cambot[FNAMESIZE];
char Cambot_out[FNAMESIZE];

#ifdef RCD
int num_distress;

/* the following copied out of BRM data.c */

int num_distress;

     /* the index into distmacro array should correspond 
   	with the correct dist_type */
struct  dmacro_list distmacro[] = {
	{ 'X', "no zero", "this should never get looked at" },
	{ 't', "taking", " %T%c->%O (%S) Carrying %a to %l%?%n>-1%{ @ %n%}\0"},
	{ 'o', "ogg", " %T%c->%O Help Ogg %p at %l\0" },
	{ 'b', "bomb", " %T%c->%O %?%n>4%{bomb %l @ %n%!bomb%}\0"},
	{ 'c', "space_control", " %T%c->%O Help Control at %L\0" },
	{ '1', "save_planet", " %T%c->%O Emergency at %L!!!!\0" },
	{ '2', "base_ogg", " %T%c->%O Sync with --]> %g <[-- OGG ogg OGG base!!\0" },
	{ '3', "help1", " %T%c->%O Help me! %d%% dam, %s%% shd, %f%% fuel %a armies.\0" },
	{ '4', "help2", " %T%c->%O Help me! %d%% dam, %s%% shd, %f%% fuel %a armies.\0" },
	{ 'e', "escorting", " %T%c->%O ESCORTING %g (%d%%D %s%%S %f%%F)\0" },
	{ 'p', "ogging", " %T%c->%O Ogging %h\0" },
	{ 'm', "bombing", " %T%c->%O Bombing %l @ %n\0" },
	{ 'l', "controlling", " %T%c->%O Controlling at %l\0" },
	{ '5', "asw", " %T%c->%O Anti-bombing %p near %b.\0" } ,
	{ '6', "asbomb", " %T%c->%O DON'T BOMB %l. Let me bomb it (%S)\0" } ,
	{ '7', "doing1", " %T%c->%O (%i)%?%a>0%{ has %a arm%?%a=1%{y%!ies%}%} at %l.  (%d%% dam, %s%% shd, %f%% fuel\0)" } ,
	{ '8', "doing2", " %T%c->%O (%i)%?%a>0%{ has %a arm%?%a=1%{y%!ies%}%} at %l.  (%d%% dam, %s%% shd, %f%% fuel)\0" } ,
	{ 'f', "free_beer", " %T%c->%O %p is free beer\0" },
	{ 'n', "no_gas", " %T%c->%O %p @ %l has no gas\0" },
	{ 'h', "crippled", " %T%c->%O %p @ %l crippled\0" },
	{ '9', "pickup", " %T%c->%O %p++ @ %l\0" },
	{ '0', "pop", " %T%c->%O %l%?%n>-1%{ @ %n%}!\0"},
	{ 'F', "carrying", " %T%c->%O %?%S=SB%{Your Starbase is c%!C%}arrying %?%a>0%{%a%!NO%} arm%?%a=1%{y%!ies%}.\0" },
	{ '@', "other1", " %T%c->%O (%i)%?%a>0%{ has %a arm%?%a=1%{y%!ies%}%} at %l. (%d%%D, %s%%S, %f%%F)\0" },
	{ '#', "other2", " %T%c->%O (%i)%?%a>0%{ has %a arm%?%a=1%{y%!ies%}%} at %l. (%d%%D, %s%%S, %f%%F)\0" },
	{ 'E', "help", " %T%c->%O Help(%S)! %s%% shd, %d%% dmg, %f%% fuel,%?%S=SB%{ %w%% wtmp,%!%}%E%{ ETEMP!%}%W%{ WTEMP!%} %a armies!\0" },
	{ '\0', '\0', '\0'},
};
#endif



/*------------------------------------------------------------------------

 Stolen direct from the Paradise 2.2.6 server code 
 This code is used to set shipvalues from .sysdef

 Added 1/9/94 by Steve Sheldon
------------------------------------------------------------------------*/

#define OFFSET_OF(field)        ( (char*)(&((struct ship*)0)->field) -\
                          (char*)0)

struct field_desc ship_fields[] = {
	{"turns",		FT_INT,   OFFSET_OF (s_turns)},
	{"accs",		FT_SHORT, OFFSET_OF (s_accs)},
	{"torpdamage",		FT_SHORT, OFFSET_OF (s_torpdamage)},
	{"phaserdamage",	FT_SHORT, OFFSET_OF (s_phaserdamage)},
	{"phaserfuse",		FT_SHORT, OFFSET_OF (s_phaserfuse)},
	{"plasmadamage",	FT_SHORT, OFFSET_OF (s_plasmadamage)},
	{"torpspeed",		FT_SHORT, OFFSET_OF (s_torpspeed)},
	{"torpfuse",		FT_SHORT, OFFSET_OF (s_torpfuse)},
	{"torpturns",		FT_SHORT, OFFSET_OF (s_torpturns)},
	{"plasmaspeed",		FT_SHORT, OFFSET_OF (s_plasmaspeed)},
	{"plasmafuse",		FT_SHORT, OFFSET_OF (s_plasmafuse)},
	{"plasmaturns",		FT_SHORT, OFFSET_OF (s_plasmaturns)},
	{"maxspeed",		FT_INT,   OFFSET_OF (s_maxspeed)},
	{"repair",              FT_SHORT, OFFSET_OF (s_repair)},
	{"maxfuel",		FT_INT,   OFFSET_OF (s_maxfuel)},
	{"torpcost",		FT_SHORT, OFFSET_OF (s_torpcost)},
	{"plasmacost",		FT_SHORT, OFFSET_OF (s_plasmacost)},
	{"phasercost",		FT_SHORT, OFFSET_OF (s_phasercost)},
	{"detcost",		FT_SHORT, OFFSET_OF (s_detcost)},
	{"warpcost",		FT_SHORT, OFFSET_OF (s_warpcost)},
	{"cloakcost",		FT_SHORT, OFFSET_OF (s_cloakcost)},
	{"recharge",		FT_SHORT, OFFSET_OF (s_recharge)},
	{"accint",		FT_INT,   OFFSET_OF (s_accint)},
	{"decint",		FT_INT,   OFFSET_OF (s_decint)},
	{"maxshield",		FT_INT,   OFFSET_OF (s_maxshield)},
	{"maxdamage",		FT_INT,   OFFSET_OF (s_maxdamage)},
	{"maxegntemp",		FT_INT,   OFFSET_OF (s_maxegntemp)},
	{"maxwpntemp",		FT_INT,   OFFSET_OF (s_maxwpntemp)},
	{"egncoolrate",		FT_SHORT, OFFSET_OF (s_egncoolrate)},
	{"wpncoolrate",		FT_SHORT, OFFSET_OF (s_wpncoolrate)},
	{"maxarmies",		FT_SHORT, OFFSET_OF (s_maxarmies)},
	{"width",		FT_SHORT, OFFSET_OF (s_width)},
	{"height",		FT_SHORT, OFFSET_OF (s_height)},
	{"type",		FT_SHORT, OFFSET_OF (s_type)},
	{"mass",		FT_SHORT, OFFSET_OF (s_mass)},
	{"tractstr",		FT_SHORT, OFFSET_OF (s_tractstr)},
	{"tractrng",		FT_FLOAT, OFFSET_OF (s_tractrng)},
	{0}
};

#ifdef FEATURE_PACKETS
int	F_client_feature_packets = 0;
#endif

int F_ship_cap      = 0;
int F_cloak_maxwarp = 0;
#ifdef RCD
int F_rc_distress   = 0;
#endif
int F_self_8flags   = 0;
int F_19flags       = 0;	/* pack 19 flags into spare bytes */
int F_show_all_tractors = 0;
int sent_ship[NUM_TYPES];
int mute = 0;
int remoteaddr = -1;		/* inet address in net format */
int whitelisted = 0;
int blacklisted = 0;
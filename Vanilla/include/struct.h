/*
 * struct.h for the server of an xtrek socket protocol.
 */

#ifndef __INCLUDED_struct_h__
#define __INCLUDED_struct_h__

#include "copyright.h"

#ifdef LTD_STATS
#include "ltd_stats.h"
#endif

struct pqueue {        /* A player queue */
    int    enter_sem;  /* Semaphore like, is the queue active */
    int    free_slots; /* Number of remaining slots for this queue */
    int    max_slots;  /* Max number of slots this queue can have */
    int    tournmask;  /* Teams this queue can enter into */
    int    low_slot;   /* The lowest slot this queue can use */
    int    high_slot;  /* The highest slot +1 this queue can use */
    int    first;      /* The waiting person at the top of the queue */
    int    last;       /* The waiting person at the end of the queue */
    u_int  count;
    int    q_flags;    /* Some flags */
    char   q_name[QNAMESIZE]; /* The name of the queue */
};

/*
 * Queue number definitions.
 */
#define  QU_PICKUP     0    /* Queue for pickup game */
#define  QU_ROBOT      1    /* Queue for robots */
#define  QU_HOME       2    /* INL Home team queue */
#define  QU_AWAY       3    /* INL Away team queue */
#define  QU_HOME_OBS   4    /* INL Home observer queue */
#define  QU_AWAY_OBS   5    /* INL Away observer queue */
#define  QU_PICKUP_OBS 6    /* Pickup observer queue */
#define  QU_GOD        7    /* Queue for God    */
#define  QU_GOD_OBS    8    /* Queue for God as Voyeur */
#define  QU_NEWBIE_PLR 9    /* Newbie server players */
#define  QU_NEWBIE_BOT 10   /* Newbie server robots */
#define  QU_NEWBIE_OBS 11   /* Newbie server observers */
#define  QU_NEWBIE_DMN 12   /* Newbie server daemon */
#define  QU_PRET_PLR   9    /* Pre-T server players */
#define  QU_PRET_BOT   10   /* Pre-T server robots */
#define  QU_PRET_OBS   11   /* Pre-T server observers */
#define  QU_PRET_DMN   12   /* Pre-T server daemon */
/*
 * Queue flag definitions.
 */

#define  QU_OPEN      0x001	/* The queue is accepting people */
#define  QU_OBSERVER  0x002	/* It is an observer queue */
#define  QU_RESTRICT  0x004	/* Tournmask may be restricted */
#define  QU_REPORT    0x008	/* Report this queue in the queue command */
#define  QU_TOK       0x010	/* Allow use of slot t in this queue */

/*
 * Structure for people who are waiting.
 */

struct queuewait {
    int mynum;                   /* My index in the waiting array */
    int inuse;                   /* Is this record in use */
    int previous;                /* Guy before me */
    int next;                    /* Guy after me */
    u_int   count;               /* Where I am in line */
    int   w_queue;               /* What line I'm in */
    pid_t process;
    char host[MAXHOSTNAMESIZE];
};

/* End of waitq related stuff */

struct status {
    int		active;
    u_char	tourn;		/* Tournament mode? */
    /* These stats only updated during tournament mode */
    u_int	armsbomb, planets, kills, losses, time;
    /* Use long for this, so it never wraps */
    double	timeprod;
    int		gameup;
};

/* The following defines are for gameup field */
#define GU_GAMEOK 1
#define game_ok ((status->gameup) & GU_GAMEOK)
#define GU_PRACTICE 2
#define practice_mode ((status->gameup) & GU_PRACTICE)
#define GU_CHAOS 4
#define chaos ((status->gameup) & GU_CHAOS)
#define GU_PAUSED 8
#define ispaused ((status->gameup) & GU_PAUSED)
#define GU_INROBOT 16			/* INL robot is present	*/
#define GU_NEWBIE 32
#define GU_PRET 64
#define pre_t_mode ((status->gameup) & GU_PRET)
#define GU_BOT_IN_GAME 128
#define bot_in_game ((status->gameup) & GU_BOT_IN_GAME)

/* values of p_status */
#define PFREE 		   0x0000
#define POUTFIT 	   0x0001
#define PALIVE 		   0x0002
#define PEXPLODE 	   0x0003
#define PDEAD 		   0x0004
/* Status value for paradise's t-mode queue feature
#define PTQUEUE		   0x0005 */
#ifdef OBSERVERS
#define POBSERV		   0x0006           /* not really dead, but observer. */
#endif

/* bit masks of p_flags */
#define PFSHIELD	   0x0001
#define PFREPAIR	   0x0002
#define PFBOMB		   0x0004
#define PFORBIT		   0x0008
#define PFCLOAK		   0x0010
#define PFWEP		   0x0020
#define PFENG		   0x0040
#define PFROBOT		   0x0080
#define PFBEAMUP	   0x0100
#define PFBEAMDOWN	   0x0200
#define PFSELFDEST	   0x0400
#define PFGREEN		   0x0800
#define PFYELLOW	   0x1000
#define PFRED		   0x2000
#define PFPLOCK		   0x4000	/* Locked on a player */
#define PFPLLOCK	   0x8000	/* Locked on a planet */
#define PFCOPILOT	  0x10000	/* Allow copilots */
#define PFWAR		  0x20000	/* computer reprogramming for war */
#define PFPRACTR	  0x40000	/* practice type robot (no kills) */
#define PFDOCK            0x80000       /* true if docked to a starbase */
#define PFREFIT          0x100000       /* true if about to refit */
#define PFREFITTING	 0x200000	/* true if currently refitting */
#define PFTRACT  	 0x400000	/* tractor beam activated */
#define PFPRESS  	 0x800000	/* pressor beam activated */
#define PFDOCKOK	0x1000000	/* docking permission */
#define PFSEEN		0x2000000	/* seen by enemy on galactic map? */
/*#define PFCYBORG	0x4000000	a cyborg? 7/27/91 TC */
#define PFOBSERV        0x8000000       /* for observers */

#ifdef SB_TRANSWARP
#define PFTWARP	       0x40000000	/* isae -- SB transwarp */
#endif

#if defined(BASEPRACTICE) || defined(NEWBIESERVER) ||  defined(PRETSERVER)
#define PFBPROBOT      0x80000000
#endif

/* values of p_whydead */
#define KLOGIN		0x00		/* initial state */
#define KQUIT		0x01		/* Player quit */
#define KTORP		0x02		/* killed by torp */
#define KPHASER		0x03		/* killed by phaser */
#define KPLANET		0x04		/* killed by planet */
#define KSHIP		0x05		/* killed by other ship */
#define KDAEMON		0x06		/* killed by dying daemon */
#define KWINNER		0x07		/* killed by a winner */
#define KGHOST		0x08		/* killed because a ghost */
#define KGENOCIDE	0x09		/* killed by genocide */
#define KPROVIDENCE	0x0a		/* killed by a hacker */
#define KPLASMA         0x0b            /* killed by a plasma torpedo */
#define TOURNEND        0x0c            /* tournament game ended */
#define KOVER           0x0d            /* game over  */
#define TOURNSTART      0x0e            /* tournament game starting */
#define KBADBIN         0x0f            /* bad binary */
#define KTORP2          0x10            /* killed by detted torps */
#define KSHIP2          0x11            /* chain-reaction explosions */
#ifdef WINSMACK
#define KPLASMA2        0x12            /* killed by zapped plasma */
#else
#define KPLASMA2	KPLASMA		/* killed by a plasma */
#endif

struct team {
    int s_turns;	/* turns till another starbase is legal */
    int s_surrender;		/* minutes until this team surrenders */
    int s_plcount;		/* how many planets this team owns */
};

struct ship {
    int s_turns;
    short s_accs;
    short s_torpdamage;
    short s_phaserdamage;
    short s_phaserfuse;
    short s_plasmadamage;
    short s_torpspeed;
    short s_torpfuse;
    short s_torpturns;
    short s_plasmaspeed;
    short s_plasmafuse;
    short s_plasmaturns;
    int s_maxspeed;
    short s_repair;
    int s_maxfuel;
    short s_torpcost;
    short s_plasmacost;
    short s_phasercost;
    short s_detcost;
    short s_warpcost;
    short s_cloakcost;
    short s_recharge;
    int s_accint;
    int s_decint;
    int s_maxshield;
    int s_maxdamage;
    int s_maxegntemp;
    int s_maxwpntemp;
    short s_egncoolrate;
    short s_wpncoolrate;
    short s_maxarmies;
    short s_width;
    short s_height;
    short s_type;
    short s_mass;        /* to guage affect of tractor beams */
    short s_tractstr;    /* Strength of tractor beam */
    float s_tractrng;
};

/*------------------------------------------------------------------------

 Stolen direct from the Paradise 2.2.6 server code
 This structure is for use in loading shipvalues from .sysdef

 Added 1/9/94 by Steve Sheldon
------------------------------------------------------------------------*/

enum field_type {
    FT_CHAR, FT_BYTE, FT_SHORT, FT_INT, FT_LONG, FT_FLOAT, FT_STRING
};
  
struct field_desc {
    char *name;
    enum field_type type;
    int offset;
};


struct stats {

#ifdef LTD_STATS
  				/* LTD stats - see README.LTD and
                                   ltd_stats.h */

    struct ltd_stats ltd[LTD_NUM_RACES][LTD_NUM_SHIPS];

#else /* LTD_STATS */

    double st_maxkills;		/* max kills ever */
    int st_kills;		/* how many kills */
    int st_losses;		/* times killed */
    int st_armsbomb;		/* armies bombed */
    int st_planets;		/* planets conquered */
    int st_ticks;		/* Ticks I've been in game */
    int st_tkills;		/* Kills in tournament play */
    int st_tlosses;		/* Losses in tournament play */
    int st_tarmsbomb;		/* Tournament armies bombed */
    int st_tplanets;		/* Tournament planets conquered */
    int st_tticks;		/* Tournament ticks */
				/* SB stats are entirely separate */
    int st_sbkills;		/* Kills as starbase */
    int st_sblosses;		/* Losses as starbase */
    int st_sbticks;		/* Time as starbase */
    double st_sbmaxkills;       /* Max kills as starbase */

#endif /* LTD_STATS */

    LONG st_lastlogin;		/* Last time this player was played */
    int st_flags;		/* Misc option flags */
    char st_keymap[96];		/* keymap for this player */
    int st_rank;		/* Ranking of the player */
#ifdef GENO_COUNT
    int st_genos;		/* # of winning genos */
#endif

#ifndef LTD_STATS
# ifdef ROLLING_STATS

    int st_okills;		/* old t-mode kills */
    int st_olosses;		/* old t-mode losses */
    int st_oarmsbomb;		/* old t-mode armies bombed */
    int st_oplanets;		/* old t-mode planets taken */
    int st_oticks;		/* old t-mode ticks */

    int st_hkills;              /* historical t-mode kills */
    int st_hlosses;             /* historical t-mode losses */
    int st_harmsbomb;           /* historical t-mode armies bombed */
    int st_hplanets;            /* historical t-mode planets taken */
    int st_hticks;              /* historical t-mode ticks */

    int st_rollingslot;		/* current rolling slot number */

    struct st_rolling
    {
	int st_rkills;		/* rolling t-mode kills */
	int st_rlosses;
	int st_rarmsbomb;
	int st_rplanets;
	int st_rticks;
    } st_rolling[ROLLING_STATS_SLOTS];

# endif /* ROLLING_STATS */

#endif /* LTD_STATS */
};

#define ST_MAPMODE      0x001
#define ST_NAMEMODE     0x002
#define ST_SHOWSHIELDS  0x004
#define ST_KEEPPEACE    0x008
#define ST_SHOWLOCAL    0x010      /* two bits for these two */
#define ST_SHOWGLOBAL   0x040
#define ST_CYBORG	0x100

#define ST_INITIAL ST_MAPMODE+ST_NAMEMODE+ST_SHOWSHIELDS+ \
                   ST_KEEPPEACE+ST_SHOWLOCAL*2+ST_SHOWGLOBAL*2;

struct player {
    int p_no;
    int p_updates;		/* Number of updates ship has survived */
    int p_status;		/* Player status */
    u_int p_flags;	        /* Player flags */
    char p_name[NAME_LEN];      /* Player handle, i.e. "Wreck" */
    char p_login[NAME_LEN];     /* Login name of player's account */
    char p_monitor[NAME_LEN];	/* Monitor being played on */
    char p_longname[NAME_LEN+6];/* Name plus (mapchars); i.e.  "Wreck (R0)" */
    char p_mapchars[3];		/* Cache for map window image, i.e. "R0" */
    struct ship p_ship;		/* Personal ship statistics */
    int p_x;
    int p_y;
    u_char p_dir;	/* Real direction */
    u_char p_desdir;	/* desired direction */
    int p_subdir;		/* fraction direction change */
    int p_speed;		/* Real speed */
    short p_desspeed;		/* Desired speed */
    int p_subspeed;		/* Fractional speed */
    short p_team;			/* Team I'm on */
    int p_damage;		/* Current damage */
    int p_subdamage;		/* Fractional damage repair */
    int p_shield;		/* Current shield power */
    int p_subshield;		/* Fractional shield recharge */
    short p_cloakphase;		/* Drawing stage of cloaking engage/disengage. */
    short p_ntorp;		/* Number of torps flying */
    short p_nplasmatorp;        /* Number of plasma torps active */
    char p_hostile;		/* Who my torps will hurt */
    char p_swar;		/* Who am I at sticky war with */
    char p_war;                 /* (p_hostile | p_swar) */
    signed char p_lastseenby;   /* Player# of player who saw me last */
    float p_kills;		/* Enemies killed */
    short p_planet;		/* Planet orbiting or locked onto */
    short p_playerl;		/* Player locked onto */
    short p_armies;	
    int p_fuel;
    short p_explode;		/* Keeps track of final explosion */
    int p_etemp;
    short p_etime;
    int p_wtemp;
    short p_wtime;
    short p_whydead;		/* Tells you why you died */
    short p_whodead;		/* Tells you who killed you */
    struct stats p_stats;	/* player statistics */
#ifdef LTD_STATS
    struct ltd_history p_hist;	/* player event history */
#endif
    short p_genoplanets;	/* planets taken since last genocide */
    short p_genoarmsbomb;	/* armies bombed since last genocide */
    short p_planets;		/* planets taken this game */
    short p_armsbomb;		/* armies bombed this game */
    int p_ghostbuster;
    int p_docked;		/* If starbase, # docked to, else pno base host */
    int p_port[4];		/* If starbase, pno of ship docked to that port,
				   else p_port[0] = port # docked to on host.   */
    short p_tractor;		/* What player is in tractor lock */
    int p_pos;			/* My position in the player file */
    int w_queue;		/* Waitqueue of my team */
#ifdef FULL_HOSTNAMES
    char p_full_hostname[MAXHOSTNAMESIZE];	/* full hostname 4/13/92 TC */
#endif
#ifdef PING
    int  p_avrt;               /* average round trip time */
    int  p_stdv;               /* standard deviation in round trip time */
    int  p_pkls_c_s;           /* packet loss (client to server) */
    int  p_pkls_s_c;           /* packet loss (server to client) */
#endif
    int p_timerdelay;           /* updates per second */
    pid_t p_process;            /* process id number */
#if defined(BASEPRACTICE) || defined(NEWBIESERVER) ||  defined(PRETSERVER)
    /* robot ogger variables */
    int p_df;			/* defense (0 unknown, 1 worst, 100 best) */
    int p_tg;			/* target+1 */
#endif
#ifdef VOTING
    time_t voting[PV_TOTAL];	/* voting array */
#endif
    int p_candock;              /* is this player allowed to dock onto SB */
    int p_transwarp;		/* flags base must have to allow transwarp */
};

struct statentry {
    char name[NAME_LEN], password[NAME_LEN];
    struct stats stats;
};


/*
 * Torpedo states   */
#define TFREE 0       /* Not in use */
#define TMOVE 1       /* Normal operation */
#define TEXPLODE 2    /* In explode sequence */
#define TDET 3        /* Has been detted, but not yet exploding */
#define TOFF 4        /* Has been turned off by owner, but not yet freed */

/*
 * Types of torpedoes */
#define TPHOTON    1  
#define TPLASMA    2

/*
 * Torpedo attributes */
#define TWOBBLE        0x001  /* Randomly change direction slightly */
#define TVECTOR        0x002  /* Add firing ship's speed vector to speed */
#define TOWNERSAFE     0x004  /* Never damage owner */
#define TOWNTEAMSAFE   0x008  /* Don't damage owner's team */
#define TDETTERSAFE    0x010  /* Don't damage detting player */
#define TDETTEAMSAFE   0x020  /* Don't damage detting player's team */

struct torp {
  int t_status;     /* State information */
  int t_type;       /* Currently only TPlasma and TPhoton */
  int t_attribute;  /* Attributes   */
  int t_owner;      /* Owning player number */	
  int t_x;          /* X Location  */
  int t_y;          /* Y Location  */
  int t_turns;      /* Rate of change of direction if tracking */
  int t_damage;     /* Explosion damage for direct hit */
  int t_gspeed;     /* Moving speed, in galactic coordinate units */
  int t_fuse;       /* Ticks remaining in current state */
  u_char t_dir;     /* Direction the torp is currently going */
  u_char t_war;     /* Set of enemy teams */
  u_char t_team;    /* Set of owning team (singleton) */
  char t_whodet;    /* Player number of player who detonated, or NODET */
#if 0
  int t_no;         /* Index in owner's torp array.  Not used */
  int t_speed;      /* Moving speed, in player units.  Not used */
#endif
};


#define PHFREE 0x00
#define PHHIT  0x01	/* When it hits a person */
#define PHMISS 0x02
#define PHHIT2 0x04	/* When it hits a photon */

struct phaser {
    int ph_status;		/* What it's up to */
    u_char ph_dir;		/* direction */
    int ph_target;		/* Who's being hit (for drawing) */
    int ph_x, ph_y;		/* For when it hits a torp */
    int ph_fuse;		/* Life left for drawing */
    int ph_damage;		/* Damage inflicted on victim */
};

/* An important note concerning planets:  The game assumes that
    the planets are in a 'known' order.  Ten planets per team,
    the first being the home planet.
*/

/* the lower bits represent the original owning team */
#define PLREPAIR 0x010
#define PLFUEL 0x020
#define PLAGRI 0x040		
#define PLREDRAW 0x080		/* Player close for redraw */
#define PLHOME 0x100		/* home planet for a given team */
#define PLCOUP 0x200		/* Coup has occured */
#define PLCHEAP 0x400		/* Planet was taken from undefended team */
#define PLCORE 0x800		/* A core world planet */

struct planet {
    int pl_no;
    int pl_flags;		/* State information */
    int pl_owner;
    int pl_x;
    int pl_y;
    char pl_name[NAME_LEN];
    int pl_namelen;		/* Cuts back on strlen's */
    int pl_armies;
    int pl_info;		/* Teams which have info on planets */
    int pl_deadtime;		/* Time before planet will support life */
    int pl_couptime;		/* Time before coup may take place */
};

#define MVALID 0x01
#define MGOD   0x10 /* this is the biggest MFROM flag - can't overlap this one*/

/* order flags by importance (0x100 - 0x400) */
/* restructuring of message flags to squeeze them all into 1 byte - jmn */
/* hopefully quasi-back-compatible:
   MVALID, MINDIV, MTEAM, MALL, MGOD use up 5 bits. this leaves us 3 bits.
   since the server only checks for those flags when deciding message
   related things and since each of the above cases only has 1 flag on at
   a time we can overlap the meanings of the flags */

#define MINDIV 0x02
/* these go with MINDIV flag */
#ifdef STDBG
#define MDBG   0x20
#endif
#ifdef FEATURES
#define MCONFIG 0x40
#endif
#define MDIST  0x60
#define DISTR  0x40 

#define MTEAM  0x04
/* these go with MTEAM flag */
#define MTAKE  0x20
#define MDEST  0x40
#define MBOMB  0x60
#define MCOUP1 0x80
#define MCOUP2 0xA0 
#define MDISTR 0xC0     /* flag distress messages - client thing really but
                           stick it in here for consistency */

#define MALL   0x08
/* these go with MALL flag */
#define MGENO  0x20     /* MGENO is not used in INL server but belongs here */
#define MCONQ  0x20     /* not enough bits to distinguish MCONQ from MGENO :( */
#define MKILLA 0x40
#define MKILLP 0x60
#define MKILL  0x80
#define MLEAVE 0xA0
#define MJOIN  0xC0
#define MGHOST 0xE0

#define MWHOMSK  0x1f   /* mask with this to find who msg to */
#define MWHATMSK 0xe0   /* mask with this to find what message about */

#define MMASK  0x3F0

/* Client to Server macros.  BE AWARE that some values will not work correctly.
   Only values that ARE NOT used in handleMessageReq() in socket.c are 
   acceptable.
*/

#define FMMACRO 0x80	/* From client only. Used to tag multi-line macros */
#define MMACRO 0x100	/* Server only. Used to tag multi-line macros */

struct message {
    int m_no;
    int m_flags;
    int m_time;
    int m_recpt;
    char m_data[MSG_LEN];
    int m_from;
    int args[8];        /* for SP_S_WARNING short messaging */
};

/* message control structure */

struct mctl {
    int mc_current;
};

/* This is a structure used for objects returned by mouse pointing */

#define PLANETTYPE 0x1
#define PLAYERTYPE 0x2

struct obtype {
    int o_type;
    int o_num;
};

struct rank {
    float hours, ratings, defense;
    char *name;
};

struct memory {
    struct player	players[MAXPLAYER];
    struct torp		torps[MAXPLAYER * (MAXTORP + MAXPLASMA)];
    struct status	status[1];
    struct planet	planets[MAXPLANETS];
    struct phaser	phasers[MAXPLAYER];
    struct mctl		mctl[1];
    struct message	messages[MAXMESSAGE];
    struct team		teams[MAXTEAM + 1];
    struct ship		shipvals[NUM_TYPES];
    struct pqueue       queues[MAXQUEUE];
    struct queuewait    waiting[MAXWAITING];
};

#ifdef RSA
struct rsa_key {
    u_char client_type[KEY_SIZE];
    u_char architecture[KEY_SIZE];
    u_char global[KEY_SIZE];
    u_char public[KEY_SIZE];
};
#endif

#ifdef RCD

/* from BRM struct.h */
struct distress {
    u_char sender;
    u_char dam, shld, arms, wtmp, etmp, fuelp, sts;
    u_char wtmpflag, etempflag, cloakflag, distype, macroflag, ttype;
    u_char close_pl, close_en, target, tclose_pl, tclose_en, pre_app, i;
    u_char close_j, close_fr, tclose_j, tclose_fr;
    u_char cclist[6]; /* allow us some day to cc a message 
			        up to 5 people */
        /* sending this to the server allows the server to do the cc action */
        /* otherwise it would have to be the client ... less BW this way */
    char preappend[80]; /* text which we pre or append */
};

struct dmacro_list {
    char c;
    char *name;
    char *macro;
};

enum dist_type {
        /* help me do series */
        take=1, ogg, bomb, space_control, help1, help2, help3, help4,

        /* doing series */
        escorting, ogging, bombing, controlling, doing1, doing2, doing3, doing4,

        /* other info series */
        free_beer, /* ie. player x is totally hosed now */
        no_gas, /* ie. player x has no gas */
        crippled, /* ie. player x is way hurt but may have gas */
        pickup, /* player x picked up armies */
        pop, /* there was a pop somewhere */
        carrying, /* I am carrying */
        other1, other2,

        /* just a generic distress call */
        generic };

        /* help1-4, doing1-4, and other1-2 are for future expansion */

enum target_type { none, planet, player };
/* The General distress has format:

A 'E' will do byte1-8 inclusive. Macros can do more but haven't been
implemented that way yet.

   byte1: xxyzzzzz
        where zzzzz is dist_type, xx is target_type and y is 1 if this is
        a macro and not just a simple distress (a simple distress will ONLY
        send ship info like shields, armies, status, location, etc.)

   byte2: 1fff ffff - f = percentage fuel remaining (0-100)
   byte3: 1ddd dddd - % damage
   byte4: 1sss ssss - % shields remaining
   byte5: 1eee eeee - % etemp
   byte6: 1www wwww - % wtemp
   byte7: 100a aaaa - armies carried
   byte8: (lsb of me->p_status) & 0x80
   byte9: 1ppp pppp - planet closest to me
   byte10: 1eee eeee - enemy closest to me

   Byte 11 and on are only sent if y (from byte 1) is = 1
        although even for simplest case (y=0) we should send byte14+ = 0x80
        and then a null so that in future we can send a cc list and 
	pre/append text
   byte11: 1ttt tttt - target (either player number or planet number)
   byte12: 1ppp pppp - planet closest to target
   byte13: 1eee eeee - enemy closest to target
   byte14+: cc list (each player to cc this message to is 11pp ppp)
        cc list is terminated by 1000 0000 or 0100 0000 )
                                 pre-pend     append
   byte15++: the text to pre or append .. depending on termination above.
             text is null terminated and the last thing in this distress
*/

#endif /* RCD */

struct command_handler {
    char *command;
    int tag;
    char *desc;
    int (*handler)();
};

#ifdef VOTING
struct vote_handler {
    char *type;
    int tag;
    int minpass;
    int start;
    char *desc;
    int frequency;
    int (*handler)();
};

#define VC_ALL     0x0001   /* Majority Vote */
#define VC_TEAM    0x0002   /* Team Vote     */
#define VC_GLOG    0x0010   /* Write Votes to God Log */
#define VC_PLAYER  0x0020   /* Each player can be voted on, like eject */


#endif

#endif /* __INCLUDED_struct_h__ */
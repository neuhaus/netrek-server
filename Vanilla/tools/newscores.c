/*
 *  Kevin P. Smith      12/05/88
 *
 *  Takes the output of scores A and generates the player file.
 *  (Also creates the .GLOBAL file)
 *  This program reads stdin for its data.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/file.h>
#include <pwd.h>
#include "defs.h"
#include INC_FCNTL
#include "struct.h"
#include "data.h"

#ifdef LTD_STATS

void main(void) {

  printf("newscores: This program does not work with LTD_STATS\n");
  return;

}

#else

#define MAXBUFFER 512

struct statentry play_entry;
struct rawdesc *output=NULL;
char mode;

extern void getpath(void);

static void usage(void);

static void trimblanks2(char *str)
{
    *str=0;
    str--;
    while (*str==' ') {
	*str=0;
	str--;
    }
    if (*str=='_') *str=0;
}

static void trimblanks(char *str)
{
    *str=0;
    str--;
    while (*str==' ') {
	*str=0;
	str--;
    }
}

/* ARGSUSED */
main(argc, argv)
int argc;
char **argv;
{
    int fd;
    char buf[MAXBUFFER];

    if (argc>1) usage();
    getpath();
    printf("Warning:  If you do not know how to use this program, break it now!\n");
    status=(struct status *) malloc(sizeof(struct status));
    scanf("%ld %d %d %d %d %lf\n", 
	&status->time, 
	&status->planets, 
	&status->armsbomb, 
	&status->kills, 
	&status->losses,
	&status->timeprod);

#ifdef CONVERT 
    status->planets*=3;
#endif

    fd = open(Global, O_WRONLY|O_CREAT|O_TRUNC, 0600);
    if (fd < 0) {
	printf("Cannot open the global file %s\n", Global);
	exit(0);
    }
    write(fd, (char *) status, sizeof(struct status));
    close(fd);

    fd = open(PlayerFile, O_WRONLY|O_CREAT|O_TRUNC, 0600);
    if (fd < 0) {
	perror(PlayerFile);
	exit(1);
    }

    while (fgets(buf, MAXBUFFER, stdin)) {
	if (strlen(buf) > 0) buf[strlen(buf)-1] = '\0';
	trimblanks2(buf+16);
	trimblanks(buf+33);
	trimblanks(buf+129);
	strcpy(play_entry.name, buf);
	strcpy(play_entry.password, buf+17);
	strcpy(play_entry.stats.st_keymap, buf+34);
#ifdef GENO_COUNT
	sscanf(buf+130, " %d %lf %d %d %d %d %d %d %d %d %d %d %d %d %d %lf %ld %d %d\n",
#else
	sscanf(buf+130, " %d %lf %d %d %d %d %d %d %d %d %d %d %d %d %d %lf %ld %d\n",
#endif
	    &play_entry.stats.st_rank,
#ifdef LTD_STATS
	    &play_entry.stats.ltd[0].kills.max,
	    &play_entry.stats.ltd[0].kills.total,
	    &play_entry.stats.ltd[0].deaths.total,
	    &play_entry.stats.ltd[0].bomb.armies,
	    &play_entry.stats.ltd[0].planets.taken,
	    &play_entry.stats.ltd[0].ticks.total,
	    &play_entry.stats.ltd[0].kills.total,
	    &play_entry.stats.ltd[0].deaths.total,
	    &play_entry.stats.ltd[0].bomb.armies,
	    &play_entry.stats.ltd[0].planets.taken,
	    &play_entry.stats.ltd[0].ticks.total,
	    &play_entry.stats.ltd[LTD_SB].kills.total,
	    &play_entry.stats.ltd[LTD_SB].deaths.total,
	    &play_entry.stats.ltd[LTD_SB].ticks.total,
	    &play_entry.stats.ltd[LTD_SB].kills.max,
#else
	    &play_entry.stats.st_maxkills,
	    &play_entry.stats.st_kills,
	    &play_entry.stats.st_losses,
	    &play_entry.stats.st_armsbomb,
	    &play_entry.stats.st_planets,
	    &play_entry.stats.st_ticks,
	    &play_entry.stats.st_tkills,
	    &play_entry.stats.st_tlosses,
	    &play_entry.stats.st_tarmsbomb,
	    &play_entry.stats.st_tplanets,
	    &play_entry.stats.st_tticks,
	    &play_entry.stats.st_sbkills,
	    &play_entry.stats.st_sblosses,
	    &play_entry.stats.st_sbticks,
	    &play_entry.stats.st_sbmaxkills,
#endif
	    &play_entry.stats.st_lastlogin,
#ifdef GENO_COUNT
	    &play_entry.stats.st_flags,
	    &play_entry.stats.st_genos);
#else
	    &play_entry.stats.st_flags);
#endif

#ifndef LTD_STATS
#ifdef CONVERT
	play_entry.stats.st_planets*=3;
	play_entry.stats.st_tplanets*=3;
#endif
#endif

	write(fd, (char *) &play_entry, sizeof(struct statentry));
    }
    return 1;		/* satisfy lint */
}

static void usage(void)
{
    printf("Usage: newscores < scoredb\n");
    printf("This program takes input of the form generated by the 'scores A'\n");
    printf(" command.  This allows you to create an ascii form of the database\n");
    printf(" with 'scores A > scoredb', then you can edit it as needed, then you\n");
    printf(" can issue a 'newscores < scoredb' command to restore the database.\n");
    printf("Note:  The form of the output from the 'scores A' command is very\n");
    printf(" specific.  Be sure that the input stream matches it very accurately\n");
    exit(0);
}

#endif /* LTD_STATS */
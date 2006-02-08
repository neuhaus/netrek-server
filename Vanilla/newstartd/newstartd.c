/* 	$Id: newstartd.c,v 1.1 2005/03/21 05:23:43 jerub Exp $	 */

#ifndef lint
static char vcid[] = "$Id: newstartd.c,v 1.1 2005/03/21 05:23:43 jerub Exp $";
#endif /* lint */

/*
 *   There's a really bad hack in newstartd.c that refuses more than 1
 *   connection from any hostname.  It's bad because it uses /usr/ucb/netstat
 *   to figure out who's connected.
 *
 *   At this point, only the hostname of the caller is available.  The
 *   daemon in theory has access to both user name and (partial) hostname,
 *   so that's where multiple logins should be denied.
 *
 *   To bypass this check, create the file defined by NoCount_File in
 *   newaccess.c.  Usually .nocount in LIBDIR.  Set by default normally.
 *
 *   4/13/92
 *
 *   Derived from distribution startd.c and xtrekII.sock.c (as of ~1/14/91).
 *
 *   Installation:  update the Prog and LogFile variables below, and
 *     set the default port appropriately (presently 2592).  Set debug=1
 *     to run this under dbx.  Link this with access.o and subnet.o.
 *  
 *   Summary of changes:
 *   - subsumes xtrekII.sock into startd (this calls ntserv directly to
 *       reduce OS overhead)
 *   - some debugger support code added
 *   - uses reaper() to keep process table clean (removes <exiting> procs)
 *
 *   8/16/91 Terence Chang
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "defs.h"
#include INC_STRINGS
#include INC_FCNTL
#include "data.h"
#include "proto.h"
#define MVERS
#include "version.h"
#include "patchlevel.h"

int restart;		/* global flag, set by SIGHUP, cleared by read	*/
int debug = 0;		/* programmers' debugging flag			*/

int get_connection(int);
int read_portfile(char *);
void deny(void);
void statistics (int, int);
void process(int);
void reaper(int);
void hangup(int);
void putpid(void);

extern int host_access(char *);
 
#define SP_BADVERSION 21
struct badversion_spacket {
  char type;
  char why;
  char pad2;
  char pad3;
};

#define MAXPROG 16			/* maximum ports to listen to	*/
#define MAXPROCESSES 8+MAXPLAYER+TESTERS+MAXWAITING
					/* maximum processes to create	*/

static int active;			/* number of processes running	*/

static
struct progrecord {			/* array of data read from file	*/
  unsigned short port;			/* port number to listen on	*/
  int sock;				/* socket created on port or -1	*/
  int nargs;				/* number of arguments to prog	*/
  char prog[512];			/* program to start on connect	*/
  char progname[512];			/* more stuff for exec()	*/
  char arg[4][64];			/* arguments for program	*/
  int internal;				/* ports handled without fork()	*/
  int accepts;				/* count of accept() calls	*/
  int denials;				/* count of deny() calls	*/
  int forks;				/* count of fork() calls	*/
  unsigned long addr;		/* address to bind() to		*/
} prog[MAXPROG];

extern char peerhostname[];		/* defined in newaccess.c	*/
int fd;					/* log file file descriptor	*/


/* sigaction() is a POSIX signal function.  On some systems, it is
   incompatible with SysV/BSD signals.  Some SysV signals provide
   sigaction(), others don't.  If you get odd signal behavior, undef
   this at the expense of zombie processes.  -da */

#undef REAPER_HANDLER
/* prevent child zombie processes -da */


#ifdef REAPER_HANDLER

/* some lame OS's don't support SA_NOCLDWAIT */
#ifndef SA_NOCLDWAIT
#define SA_NOCLDWAIT	0
#endif

#ifdef linux
#define sa_sigaction sa_restorer
#endif

void handle_reaper(void) {
  struct sigaction action;

  action.sa_flags = SA_NOCLDWAIT;
  action.sa_handler = reaper;
  sigemptyset(&(action.sa_mask));
  action.sa_sigaction = NULL;
  sigaction(SIGCHLD, &action, NULL);
}

#endif

int main (int argc, char *argv[])
{
  char *portfile = N_PORTS;
  int port_idx = -1;
  int num_progs = 0;
  int i;
  pid_t pid;
  FILE *file;

  active = 0;
  getpath ();

  /* if someone tries to ask for help, give 'em it */
  if (argc == 2 && argv[1][0] == '-') {
    fprintf (stderr, "Usage: %s [portfile] [debug]\n", argv[0] );
    fprintf (stderr, "Usage: %s stop\n", argv[0] );
    exit (1);
  }

  /* fetch our previous pid if available */
  file = fopen (N_NETREKDPID, "r");
  if (file != NULL) {
    fscanf (file, "%d", &pid);
    fclose (file);
  } else {
    /* only a total lack of the file is acceptable */
    if (errno != ENOENT) {
      perror (N_NETREKDPID);
      exit(1);
    }
  }

  /* check for a request to stop the listener */
  if (argc == 2) {
    if (!strcmp (argv[1], "stop")) {
      if (file != NULL) {
	if (kill (pid, SIGINT) == 0) {
	  remove (N_NETREKDPID);
	  fprintf (stderr, "netrekd: stopped pid %d\n", pid);
	  exit (0);
	}
	fprintf (stderr, "netrekd: cannot stop, pid %d\n, may be already stopped", pid);
	perror ("kill");
	exit (1);
      }
      fprintf (stderr, "netrekd: cannot stop, no %s file\n", N_NETREKDPID);
      exit (1);
    }
  }

  /* check for duplicate start */
  if (file != NULL) {
    /* there is a pid file, test the pid is valid */
    if (kill (pid, 0) == 0) {
      fprintf (stderr, "netrekd: cannot start, already running as pid %d\n", pid);
      exit (1);
    } else {
      if (errno != ESRCH) {
	fprintf (stderr, "netrekd: failed on test of pid %d existence\n", pid);
	perror ("kill");
	exit (1);
      }
    }

    /* pid was not valid but file was present, so remove it */
    remove (N_NETREKDPID);
  }

  /* allow user to specify a port file to use */
  if (argc == 2) {
    if (strcmp (argv[1], "start")) {
      portfile = argv[1];
    }
  }

  /* allow developer to ask for verbose output */
  if (argc == 3 && !strcmp(argv[2], "debug")) debug++;

  /* wander off to the place where we live */
  if (chdir (LIBDIR) != 0) {
    perror (LIBDIR);
    exit(1);
  }

  /* check file access before forking */
  if (access (portfile, R_OK) != 0) {
    perror (portfile);
    exit(1);
  }

  /* set up handlers for signals */
#ifdef REAPER_HANDLER
  handle_reaper();
#else
  SIGNAL (SIGCHLD, reaper);
#endif
  SIGNAL (SIGHUP, hangup);
  SIGNAL (SIGUSR1, SIG_IGN);

  /* open the connection log file */
  if ((fd = open (LogFile, O_CREAT | O_WRONLY | O_APPEND, 0600)) < 0) {
    perror (LogFile);
  }

  /* fork this as a daemon */
  pid = fork();
  if (pid != 0) {
    if (pid < 0) { 
      perror("fork"); 
      exit(1); 
    }
    fprintf (stderr, "netrekd: Vanilla Netrek Listener %spl%d started, pid %d,\n"
	     "netrekd: logging to %s\n", 
	     mvers, PATCHLEVEL, pid, LogFile );
    exit(0);
  }

  /* detach from terminal */
  DETACH

  /* do not propogate the log to forked processes */
  fcntl(fd, F_SETFD, FD_CLOEXEC);

  /* close our standard file descriptors and attach them to the log */
  (void) dup2 (fd, 0);
  (void) dup2 (fd, 1);
  (void) dup2 (fd, 2);

  /* set the standard streams to be line buffered (ANSI C3.159-1989) */
  setvbuf (stderr, NULL, _IOLBF, 0);
  setvbuf (stdout, NULL, _IOLBF, 0);

  /* record our process id */
  putpid ();

  /* run forever */
  for (;;) {

    /* [re-]read the port file */
    num_progs = read_portfile (portfile);
    restart = 0;

    /* run until restart requested */
    while (!restart) {
      pid_t pid;

      /* wait for one connection */
      port_idx = get_connection (num_progs);
      if (port_idx < 0) continue;
      prog[port_idx].accepts++;

      /* check this client against denied parties */
      if (host_access (NULL) == 0) {
        prog[port_idx].denials++;
	deny ();
	close (0);
	continue;
      }

      /* check for limit */
      if (active > MAXPROCESSES) {
	fprintf(stderr, 
		"netrekd: hit maximum processes, connection closed\n");
	close (0);
	continue;
      }

      /* check for internals */
      if (prog[port_idx].internal) {
	statistics (port_idx, num_progs);
	continue;
      }

      /* fork to execute the program specified by .ports file */
      pid = fork ();
      if (pid == -1) {
	perror("fork");
	close (0);
	continue;
      }

      if (pid == 0) {
	process (port_idx);		/* returns only on error */
	exit (0);
      }

      prog[port_idx].forks++;
      active++;

      if (debug) fprintf (stderr, "active++: %d: pid %d port %d\n", active,
			  pid, prog[port_idx].port);

      /* main program continues after fork, close socket to client */
      close (0);

    } /* while (!restart) */
    
    /* SIGHUP restart requested, close all listening sockets */
    fprintf (stderr, "netrekd: restarting on SIGHUP\n");

    for (i = 0; i < num_progs; i++) {
      if (prog[i].sock >= 0) close(prog[i].sock);
    }

    /* between closing the listening sockets and commencing the new ones
       there is a time during which incoming client connections are refused */
  }
}

int get_connection (int num_progs)
{
  struct sockaddr_in addr;
  struct sockaddr_in naddr;
  fd_set accept_fds;
  int len, i, st, newsock;
  
  int foo = 1;
  
  /* stage one; for each port not yet open, set up listening socket */

  /* check all ports */
  for (i = 0; i < num_progs; i++) {
    sock = prog[i].sock;
    if (sock < 0) {

      fprintf (stderr, "netrekd: port %d, %s, ", prog[i].port, prog[i].prog);
      if ((sock = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
	perror ("socket");
	sleep (1);
	return -1;
      }

      /* we can't cope with having socket zero for a listening socket */
      if (sock == 0) {
	if (debug) fprintf (stderr, "gack: don't want socket zero\n");
        if ((sock = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
	  perror ("socket");
	  sleep (1);
	  close (0);
	  return -1;
	}
	close (0);
      }
      
      /* set the listening socket to close on exec so that children
	 don't hold open the file descriptor, thus preventing us from
	 restarting netrekd. */
      
      if (fcntl(sock, F_SETFD, FD_CLOEXEC) < 0) {
	perror ("fcntl F_SETFD FD_CLOEXEC");
	close (sock);
	sleep (1);
	return -1;
      }

      /* set the listening socket to allow re-use of the address (port)
	 so that we don't have to pay the CLOSE_WAIT penalty if we need
	 to close and re-open the socket on restart. */

      if (setsockopt (sock, SOL_SOCKET, SO_REUSEADDR,
		      (char *) &foo, sizeof (int)) < 0) {
	perror ("setsockopt SOL_SOCKET SO_REUSEADDR");
	close (sock);
	sleep (1);
	return -1;
      }
      
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = prog[i].addr;
      addr.sin_port = htons (prog[i].port);
      
      if (bind (sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
	perror ("bind");
	close (sock);
	sleep (1);
	continue;
      }

      if (listen (sock, 1) < 0) {
	perror ("listen");
	close (sock);
	sleep (1);
	continue;
      }

      prog[i].sock = sock;
      fprintf (stderr, "listening fd %d, to do \"%s\" %s %s %s %s\n",
	       prog[i].sock, prog[i].progname, prog[i].arg[0],
	       prog[i].arg[1], prog[i].arg[2], prog[i].arg[3]);
      fflush (stderr);
    }
  }

  /* stage two; wait for a connection on the listening sockets */
  
  for (;;) { 				/* wait for a connection */
    st = 0;

    FD_ZERO (&accept_fds);		/* clear the file descriptor mask */
    for (i = 0; i < num_progs; i++) {	/* set bits in mask for each sock */
      sock = prog[i].sock;
      if (sock < 0) continue;
      FD_SET (sock, &accept_fds);
      st++;
    }

    /* paranoia, it could be possible that we have no sockets ready */
    if (st == 0) {
      fprintf(stderr, "netrekd: paranoia, no sockets to listen for\n");
      sleep(1);
      return -1;
    }
    
    /* block until select() indicates accept() will complete */
    st = select (32, &accept_fds, 0, 0, 0);
    if (st > 0) break;
    if (restart) return -1;
    if (errno == EINTR) continue;
    perror("select");
    sleep(1);
  }

  /* find one socket in mask that shows activity */
  for (i = 0; i < num_progs; i++) {
    sock = prog[i].sock;
    if (sock < 0) continue;
    if (FD_ISSET (sock, &accept_fds)) break;
  }

  /* none of the sockets showed activity? erk */
  if (i >= num_progs) return -1;

  len = sizeof(naddr);
    
  for (;;) {
    newsock = accept (sock, (struct sockaddr*)&naddr, &len);
    if (newsock >= 0) break;
    if (errno == EINTR) continue;
    fprintf (stderr, "netrekd: port %d, %s, ", prog[i].port, prog[i].prog);
    perror ("accept");
    sleep (1);
    return -1;
  }

  if (newsock != 0) {
    if (dup2 (newsock, 0) == -1) {
/* on solaris it has been seen that fd 0 is one of our listening sockets! */
      perror ("dup2");
    }
    close (newsock);
  }
  return i;
}

void deny(void)
{
  struct badversion_spacket packet;

  /* if log is open, write an entry to it */
  if (fd != -1) {
    time_t curtime;
    char logname[BUFSIZ];

    time (&curtime);
    sprintf (logname, "Denied %-32.32s %s",
	     peerhostname,
	     ctime(&curtime));
    write (fd, logname, strlen(logname));
  }

  /* issue a bad version packet to the client */
  packet.type = SP_BADVERSION;
  packet.why = 1;
  write (0, (char *) &packet, sizeof(packet));

  sleep (2);
}

void statistics (int port_idx, int num_progs)
{
  FILE *client;
  int i;

  if (fork() == 0) {
    static struct linger linger = {1, 500};
    setsockopt (0, SOL_SOCKET, SO_LINGER, (char *) &linger, sizeof (linger));
    client = fdopen (0, "w");
    if (client != NULL) {
      if (!strcmp (peerhostname, "localhost")) {
	fprintf (client, "port\taccepts\tdenials\tforks\n" );
	for (i=0; i<num_progs; i++) {
	  fprintf (client,
		   "%d\t%d\t%d\t%d\n",
		   prog[i].port,
		   prog[i].accepts,
		   prog[i].denials,
		   prog[i].forks );
	}
      }
      fprintf (client, "%s\nVersion %spl%d\n", vcid, mvers, PATCHLEVEL );
      fflush (client);
      if (fclose (client) < 0) perror("fclose");
    } else {
      close (0);
    }
    exit (1);
  } else {
    close (0);
  }

  /* get rid of compiler warning */
  if (port_idx) return;
}

void process (int port_idx)
{
  struct progrecord *pr;

  /* note: we are a clone */

  /* if log is open, write an entry to it, and close it */
  if (fd != -1) {
    time_t curtime;
    char logname[BUFSIZ];

    time (&curtime);
    sprintf (logname, "       %-32.32s %s",
	     peerhostname,
	     ctime(&curtime));
    write (fd, logname, strlen(logname));
    close (fd);
  }
  
  /* execute the program required */
  pr = &(prog[port_idx]);
  switch (pr->nargs) {
  case 0:
    execl (pr->prog, pr->progname, peerhostname, 0);
    break;
  case 1:
    execl (pr->prog, pr->progname, pr->arg[0], peerhostname, 0);
    break;
  case 2:
    execl (pr->prog, pr->progname, pr->arg[0], pr->arg[1], 
	   peerhostname, 0);
    break;
  case 3:
    execl (pr->prog, pr->progname, pr->arg[0], pr->arg[1], 
	   pr->arg[2], peerhostname, 0);
    break;
  case 4:
    execl (pr->prog, pr->progname, pr->arg[0], pr->arg[1], 
	   pr->arg[2], pr->arg[3], peerhostname, 0);
    break;
  default: ;
  }
  fprintf (stderr, "Error in execl! -- %s\n", pr->prog);
  perror ("execl");
  close (0);
  exit (1);
}

/* set flag requesting restart, select() will get EINTR */
void hangup (int sig)
{
  restart++;
  HANDLE_SIG (SIGHUP, hangup);

  /* get rid of compiler warning */
  if (sig) return;
}

/* accept termination of any child processes */
void reaper (int sig)
{
  int stat;
  pid_t pid;

  for (;;) {
    pid = WAIT3 (&stat, WNOHANG, 0);
    if (pid > 0) {
      active--;
      if (debug) fprintf (stderr, "active--: %d: pid %d terminated\n", 
			  active, pid);
    }
    else
        break;
  }

#ifdef REAPER_HANDLER
  /* forgot to add this.  -da */
  handle_reaper();
#else
  HANDLE_SIG (SIGCHLD, reaper);
#endif

  if (sig) return;
}

/* write process id to file to assist with automatic signalling */
void putpid(void)
{
  FILE *file;

  file = fopen(N_NETREKDPID, "w");
  if (file == NULL) return;
  fprintf (file, "%d\n", (int) getpid());
  fclose (file);
}

int read_portfile (char *portfile)
{
  FILE *fi;
  char buf[BUFSIZ], addrbuf[BUFSIZ], *port;
  int i = 0, n;
  struct stat sbuf;

  fi = fopen (portfile, "r");
  if (fi) {
    while (fgets (buf, BUFSIZ, fi)) {
	if (buf[0] == '#')
		continue;
      if ((n = sscanf (buf, "%s %s \"%[^\"]\" %s %s %s %s",
               addrbuf,
		       prog[i].prog,
		       prog[i].progname,
		       prog[i].arg[0],
		       prog[i].arg[1],
		       prog[i].arg[2],
		       prog[i].arg[3])) >= 3) {
	if (!(port = strchr(addrbuf, ':')))
	{
		prog[i].addr = INADDR_ANY;
		prog[i].port = atoi(addrbuf);
	}
	else
	{
		*port++ = '\0';
		prog[i].addr = inet_addr(addrbuf);
		prog[i].port = atoi(port);
	};
	prog[i].nargs = n-3;
	prog[i].sock = -1;
	prog[i].internal = (!strcmp (prog[i].prog, "special"));
	prog[i].accepts = 0;
	prog[i].denials = 0;
	prog[i].forks = 0;

	/* check normal entries to make sure program mode is ok */
	if ((!prog[i].internal) &&
	    (stat (prog[i].prog, &sbuf) < 0 || !(sbuf.st_mode & S_IRWXU))) {
	  fprintf (stderr, "\"%s\" not accessible or not executable.\n",
		   prog[i].prog);
	  fflush (stderr);
	} else {
	  i++;
	}
      }
    }
    fclose (fi);
  } else {
    perror (portfile);
  }
  
  /* if the .ports file is empty, at least open the standard port */
  if (!i) {
    strcpy (prog[0].prog, Prog);
    strcpy (prog[0].progname, Prog);
    prog[0].port = PORT;
    prog[0].nargs = 0;
    prog[0].sock = -1;
    prog[0].accepts = 0;
    prog[0].denials = 0;
    prog[0].forks = 0;
    i = 1;
  }

  return i;
}
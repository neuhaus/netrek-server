/*
 * db.c
 */
#include "copyright.h"
#include "config.h"

#include <stdio.h>
#include <stddef.h>
#include INC_MATH
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#ifdef PLAYER_INDEX
#include <gdbm.h>
#endif
#include "defs.h"
#include INC_STRINGS
#include INC_UNISTD
#include "struct.h"
#include "data.h"
#include "salt.h"

/* support for timing requires glib, not expected to remain */
#undef DB_TIMING
#ifdef DB_TIMING
#include <glib.h>
#endif

#ifdef PLAYER_INDEX

/* fetch the offset to a player from the index database */
static off_t db_index_fetch(char *namePick, struct statentry *player) {
  GDBM_FILE dbf;
  datum key, content;
  off_t position;

  /* open the player index database */
  dbf = gdbm_open(PlayerIndexFile, 0, GDBM_WRCREAT, 0644, NULL);
  if (dbf == NULL) {
    ERROR(1,("db.c: db_index_fetch: gdbm_open('%s'): '%s', '%s'\n",
	     PlayerIndexFile, gdbm_strerror(gdbm_errno), strerror(errno)));
    return -1;
  }
  ERROR(8,("db.c: db_index_fetch: gdbm_open('%s'): ok\n", 
	   PlayerIndexFile));

  /* fetch the database entry for this player name */
  key.dptr = namePick;
  key.dsize = strlen(namePick);
  content = gdbm_fetch(dbf, key);

  /* player index may not contain this player, that's fine */
  if (content.dptr == NULL) {
    ERROR(8,("db.c: db_index_fetch: gdbm_fetch('%s'): not found in index\n",
	     namePick));
    gdbm_close(dbf);
    return -1;
  }

  if (content.dsize != sizeof(off_t)) {
    ERROR(8,("db.c: db_index_fetch: gdbm_fetch('%s'): dsize [%d] not sizeof(off_t) [%d]\n",
	     namePick, content.dsize, sizeof(off_t)));
    gdbm_close(dbf);
    return -1;
  }

  /* return the position from the database entry */
  position = *((off_t *) content.dptr);
  free(content.dptr);
  gdbm_close(dbf);
  ERROR(8,("db.c: db_index_fetch: gdbm_fetch('%s'): index says position '%d'\n",
	   namePick, position));
  return position;
}

/* store the offset to a player into the index database */
static void db_index_store(struct statentry *player, off_t position) {
  GDBM_FILE dbf;
  datum key, content;

  /* open the player index database */
  dbf = gdbm_open(PlayerIndexFile, 0, GDBM_WRCREAT, 0644, NULL);
  if (dbf == NULL) { return; }

  /* prepare the key and content pair from name and position */
  key.dptr = player->name;
  key.dsize = strlen(player->name);
  content.dptr = (char *) &position;
  content.dsize = sizeof(position);

  /* store this key and position */
  if (gdbm_store(dbf, key, content, GDBM_REPLACE) < 0) {
    ERROR(8,("db.c: db_index_store: gdbm_store('%s' -> '%d'): %s, %s\n", 
	     player->name, position, gdbm_strerror(gdbm_errno), 
	     strerror(errno)));
    gdbm_close(dbf);
    return;
  }

  ERROR(8,("db.c: db_index_store: gdbm_store('%s' -> '%d'): ok\n", 
	   player->name, position));
  gdbm_close(dbf);
}

#endif

/* given a name, find the player in the file, return position */
int findplayer(char *namePick, struct statentry *player) {
  off_t position;
  int fd;
#ifdef DB_TIMING
  GTimer *timer = g_timer_new();
#endif

  /* open the player file */
  fd = open(PlayerFile, O_RDONLY, 0644);
  if (fd < 0) {
    ERROR(1,("db.c: findplayer: open('%s'): '%s'\n", PlayerFile, 
	     strerror(errno)));
    strcpy(player->name, namePick);
#ifdef DB_TIMING
    g_timer_destroy(timer);
#endif
    return -1;
  }

#ifdef PLAYER_INDEX
  /* use the index as a hint as to position in the player file */
  position = db_index_fetch(namePick, player);

  /* if an index entry was present, read the entry to check it's right */
  if (position != -1) {
    lseek(fd, position * sizeof(struct statentry), SEEK_SET);
    if (read(fd, (char *) player, sizeof(struct statentry)) < 0) {
      /* read failed for some reason */
      ERROR(1,("db.c: findplayer: read: '%s'\n", strerror(errno)));
      strcpy(player->name, "");
    }
    /* check the entry in the main file matches what the index said */
    if (strcmp(namePick, player->name)==0) {
      close(fd);
      ERROR(8,("db.c: findplayer: ok, '%s' is indeed at position '%d'\n", 
	       namePick, position));
#ifdef DB_TIMING
      ERROR(8,("db.c: timing, cached resolution, %f\n", g_timer_elapsed(timer, NULL)));
      g_timer_destroy(timer);
#endif      
      return position;
    }
    /* otherwise there's an inconsistency that we can recover from */
    ERROR(2,("db.c: findplayer: player index inconsistent with player file, entered name '%s', index says position '%d', but file entry name '%s'\n", namePick, position, player->name));
    /* return file to start for sequential search */
    lseek(fd, 0, SEEK_SET);
  }
#endif

  /* sequential search of player file */
  position = 0;
  while (read(fd, (char *) player, sizeof(struct statentry)) ==
	 sizeof(struct statentry)) {
    if (strcmp(namePick, player->name)==0) {
      close(fd);
#ifdef PLAYER_INDEX
      db_index_store(player, position);
#endif
      ERROR(8,("db.c: findplayer: '%s' found in sequential scan at position '%d'\n", 
	       namePick, position));
#ifdef DB_TIMING
      ERROR(8,("db.c: timing, sequential resolution, %f\n", g_timer_elapsed(timer, NULL)));
      g_timer_destroy(timer);
#endif      
      return position;
    }
    position++;
  }

  /* not found, return failure */
  close(fd);
  strcpy(player->name, namePick);
  ERROR(8,("db.c: findplayer: '%s' not found in sequential scan\n", 
	   namePick));
#ifdef DB_TIMING
  ERROR(8,("db.c: timing, sequential failure, %f\n", g_timer_elapsed(timer, NULL)));
  g_timer_destroy(timer);
#endif      
  return -1;
}

void savepass(const struct statentry* se)
{
    int fd;
    if (me->p_pos < 0) return;
    ERROR(8,("db.c: savepass: saving to position '%d'\n", me->p_pos));
    fd = open(PlayerFile, O_WRONLY, 0644);
    if (fd >= 0) {
	lseek(fd, me->p_pos * sizeof(struct statentry) +
	      offsetof(struct statentry, password), SEEK_SET);
	write(fd, &se->password, sizeof(se->password));
	close(fd);
    }
}

void changepassword (char *passPick)
{
  saltbuf sb;
  struct statentry se;
  strcpy(se.password, (char *) crypt(passPick, salt(me->p_name, sb)));
  savepass(&se);
}

void savestats(void)
{
    int fd;

    if (me->p_pos < 0) return;

#ifdef OBSERVERS
    /* Do not save stats for observers.  This is corrupting the DB. -da */
    if (Observer) return;
#endif

    fd = open(PlayerFile, O_WRONLY, 0644);
    if (fd >= 0) {
	me->p_stats.st_lastlogin = time(NULL);
	lseek(fd, me->p_pos * sizeof(struct statentry) +
	      offsetof(struct statentry, stats), SEEK_SET);
	write(fd, (char *) &me->p_stats, sizeof(struct stats));
	close(fd);
    }
}

int newplayer(struct statentry *player)
{
  int fd, offset, position;

  ERROR(8,("db.c: newplayer: adding '%s'\n", player->name));

  fd = open(PlayerFile, O_RDWR|O_CREAT, 0644);
  if (fd < 0) return -1;
  if ((offset = lseek(fd, 0, SEEK_END)) < 0) return -1;
  ERROR(8,("db.c: newplayer: lseek gave offset '%d'\n", offset));

  position = offset / sizeof(struct statentry);
  if ((offset % sizeof(struct statentry)) != 0) {
    ERROR(1,("db.c: newplayer: SEEK_END not multiple of struct statentry, truncating down\n"));
    offset = position * sizeof(struct statentry);
    ERROR(8,("db.c: newplayer: truncated offset '%d'\n", offset));
    if ((offset = lseek(fd, offset, SEEK_SET)) < 0) return -1;
  }
  write(fd, (char *) player, sizeof(struct statentry));
  close(fd);

  ERROR(8,("db.c: newplayer: sizeof '%d' offset '%d' position '%d'\n", 
	   sizeof(struct statentry), offset, position));

#ifdef PLAYER_INDEX
  /* do not create an index entry until the character name is reused,
     because not all characters are re-used */
#endif

  return position;
}
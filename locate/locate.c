/* locate -- search databases for filenames that match patterns
   Copyright (C) 1994, 96, 98, 99, 2000, 2003 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA.
*/

/* Usage: locate [options] pattern...

   Scan a pathname list for the full pathname of a file, given only
   a piece of the name (possibly containing shell globbing metacharacters).
   The list has been processed with front-compression, which reduces
   the list size by a factor of 4-5.
   Recognizes two database formats, old and new.  The old format is
   bigram coded, which reduces space by a further 20-25% and uses the
   following encoding of the database bytes:

   0-28		likeliest differential counts + offset (14) to make nonnegative
   30		escape code for out-of-range count to follow in next halfword
   128-255 	bigram codes (the 128 most common, as determined by `updatedb')
   32-127  	single character (printable) ASCII remainder

   Uses a novel two-tiered string search technique:

   First, match a metacharacter-free subpattern and a partial pathname
   BACKWARDS to avoid full expansion of the pathname list.
   The time savings is 40-50% over forward matching, which cannot efficiently
   handle overlapped search patterns and compressed path remainders.

   Then, match the actual shell glob-style regular expression (if in this form)
   against the candidate pathnames using the slower shell filename
   matching routines.

   Described more fully in Usenix ;login:, Vol 8, No 1,
   February/March, 1983, p. 8.

   Written by James A. Woods <jwoods@adobe.com>.
   Modified by David MacKenzie <djm@gnu.ai.mit.edu>.  */

#include <config.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fnmatch.h>
#include <getopt.h>

#define NDEBUG
#include <assert.h>

#if defined(HAVE_STRING_H) || defined(STDC_HEADERS)
#include <string.h>
#else
#include <strings.h>
#define strchr index
#endif

#ifdef STDC_HEADERS
#include <stdlib.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#else
extern int errno;
#endif

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#if ENABLE_NLS
# include <libintl.h>
# define _(Text) gettext (Text)
#else
# define _(Text) Text
#define textdomain(Domain)
#define bindtextdomain(Package, Directory)
#endif
#ifdef gettext_noop
# define N_(String) gettext_noop (String)
#else
/* We used to use (String) instead of just String, but apparentl;y ISO C
 * doesn't allow this (at least, that's what HP said when someone reported
 * this as a compiler bug).  This is HP case number 1205608192.  See
 * also http://gcc.gnu.org/bugzilla/show_bug.cgi?id=11250 (which references 
 * ANSI 3.5.7p14-15).  The Intel icc compiler also rejects constructs
 * like: static const char buf[] = ("string");
 */
# define N_(String) String
#endif

#include "locatedb.h"
#include <getline.h>
#include "../gnulib/lib/xalloc.h"
#include "../gnulib/lib/error.h"

/* Note that this evaluates C many times.  */
#ifdef _LIBC
# define TOUPPER(Ch) toupper (Ch)
# define TOLOWER(Ch) tolower (Ch)
#else
# define TOUPPER(Ch) (islower (Ch) ? toupper (Ch) : (Ch))
# define TOLOWER(Ch) (isupper (Ch) ? tolower (Ch) : (Ch))
#endif

typedef enum {false, true} boolean;

/* Warn if a database is older than this.  8 days allows for a weekly
   update that takes up to a day to perform.  */
#define WARN_NUMBER_UNITS (8)
/* Printable name of units used in WARN_SECONDS */
static const char warn_name_units[] = N_("days");
#define SECONDS_PER_UNIT (60 * 60 * 24)

#define WARN_SECONDS ((SECONDS_PER_UNIT) * (WARN_NUMBER_UNITS))

/* Check for existence of files before printing them out? */
int check_existence = 0;

/* What to separate the results with. */
static int separator = '\n';

char *next_element ();


/* Read in a 16-bit int, high byte first (network byte order).  */

static short
get_short (fp)
     FILE *fp;
{

  register short x;

  x = (signed char) fgetc (fp) << 8;
  x |= (fgetc (fp) & 0xff);
  return x;
}

/* Return a pointer to the last character in a static copy of the last
   glob-free subpattern in NAME,
   with '\0' prepended for a fast backwards pre-match.  */

static char *
last_literal_end (name)
     char *name;
{
  static char *globfree = NULL;	/* A copy of the subpattern in NAME.  */
  static size_t gfalloc = 0;	/* Bytes allocated for `globfree'.  */
  register char *subp;		/* Return value.  */
  register char *p;		/* Search location in NAME.  */

  /* Find the end of the subpattern.
     Skip trailing metacharacters and [] ranges. */
  for (p = name + strlen (name) - 1; p >= name && strchr ("*?]", *p) != NULL;
       p--)
    {
      if (*p == ']')
	while (p >= name && *p != '[')
	  p--;
    }
  if (p < name)
    p = name;

  if (p - name + 3 > gfalloc)
    {
      gfalloc = p - name + 3 + 64; /* Room to grow.  */
      globfree = xrealloc (globfree, gfalloc);
    }
  subp = globfree;
  *subp++ = '\0';

  /* If the pattern has only metacharacters, make every path match the
     subpattern, so it gets checked the slow way.  */
  if (p == name && strchr ("?*[]", *p) != NULL)
    *subp++ = '/';
  else
    {
      char *endmark;
      /* Find the start of the metacharacter-free subpattern.  */
      for (endmark = p; p >= name && strchr ("]*?", *p) == NULL; p--)
	;
      /* Copy the subpattern into globfree.  */
      for (++p; p <= endmark; )
	*subp++ = *p++;
    }
  *subp-- = '\0';		/* Null terminate, though it's not needed.  */

  return subp;
}

/* locate_read_str()
 *
 * Read bytes from FP into the buffer at offset OFFSET in (*BUF),
 * until we reach DELIMITER or end-of-file.   We reallocate the buffer 
 * as necessary, altering (*BUF) and (*SIZ) as appropriate.  No assumption 
 * is made regarding the content of the data (i.e. the implementation is 
 * 8-bit clean, the only delimiter is DELIMITER).
 *
 * Written Fri May 23 18:41:16 2003 by James Youngman, because getstr() 
 * has been removed from gnulib.  
 *
 * We call the function locate_read_str() to avoid a name clash with the curses
 * function getstr().
 */
static int
locate_read_str(char **buf, size_t *siz, FILE *fp, int delimiter, int offs)
{
  char * p = NULL;
  size_t sz = 0;
  int needed, nread;

  nread = getdelim(&p, &sz, delimiter, fp);
  if (nread >= 0)
    {
      assert(p != NULL);
      
      needed = offs + nread + 1;
      if (needed > (*siz))
	{
	  char *pnew = realloc(*buf, needed);
	  if (NULL == pnew)
	    {
	      return -1;	/* FAIL */
	    }
	  else
	    {
	      *siz = needed;
	      *buf = pnew;
	    }
	}
      memcpy((*buf)+offs, p, nread);
      free(p);
    }
  return nread;
}


enum visit_result
  {
    VISIT_CONTINUE = 1,  /* please call the next visitor */
    VISIT_ACCEPTED = 2,  /* accepted, call no futher callbacks for this file */
    VISIT_REJECTED = 4,  /* rejected, process next file. */
    VISIT_ABORT    = 8   /* rejected, process no more files. */
  };


typedef int (*visitfunc)(const char *filename, void *context);

struct visitor
{
  visitfunc      inspector;
  void *         context;
  struct visitor *next;
};


static struct visitor *inspectors = NULL;
static struct visitor *lastinspector = NULL;

static int
process_filename(const char *filename)
{
  int result = VISIT_CONTINUE;
  const struct visitor *p = inspectors;
  
  while ( (VISIT_CONTINUE == result) && (NULL != p) )
    {
      result = (p->inspector)(filename, p->context);
      p = p->next;
    }

  if (VISIT_CONTINUE == result)
    return VISIT_ACCEPTED;
  else
    return result;
}

static void
add_visitor(visitfunc fn, void *context)
{
  struct visitor *p = xmalloc(sizeof(struct visitor));
  p->inspector = fn;
  p->context   = context;
  p->next = NULL;
  
  if (NULL == lastinspector)
    {
      lastinspector = inspectors = p;
    }
  else
    {
      lastinspector->next = p;
    }
}



static int
visit_justprint(const char *name, void *context)
{
  (void)&context;
  fputs(name, stdout);
  putchar(separator);
  return VISIT_CONTINUE;
}

static int
visit_exists(const char *name, void *context)
{
  struct stat st;
  (void)&context;
  if (stat(name, &st) != 0)
    {
      return VISIT_REJECTED;
    }
  else
    {
      return VISIT_CONTINUE;
    }
}

static int
visit_substring_match(const char *name, void *context)
{
  const char *pattern = context;

  if (NULL != strstr(name, pattern))
    return VISIT_CONTINUE;
  else
    return VISIT_REJECTED;
}


static int
visit_globmatch_nofold(const char *name, void *context)
{
  const char *glob = context;
  if (fnmatch(glob, name, 0) != 0)
    return VISIT_REJECTED;
  else
    return VISIT_CONTINUE;
}


static int
visit_globmatch_casefold(const char *name, void *context)
{
  const char *glob = context;
  if (fnmatch(glob, name, FNM_CASEFOLD) != 0)
    return VISIT_REJECTED;
  else
    return VISIT_CONTINUE;
}


#if OLD_LOCATE
/* Print the entries in DBFILE that match shell globbing pattern PATHPART.
   Return the number of entries printed.  */

static int
old_locate (pathpart, dbfile, ignore_case)
     char *pathpart, *dbfile;
     int ignore_case;
{
  /* The pathname database.  */
  FILE *fp;
  /* An input byte.  */
  int c;
  /* Number of bytes read from an entry.  */
  int nread;

  /* true if PATHPART contains globbing metacharacters.  */
  boolean globflag;
  /* The end of the last glob-free subpattern in PATHPART.  */
  char *patend;

  /* The current input database entry.  */
  char *path;
  /* Amount allocated for it.  */
  size_t pathsize;

  /* The length of the last path.  */
  size_t last_pathlen;

  /* The length of the prefix shared with the previous database entry.  */
  int count = 0;
  /* Where in `path' to stop the backward search for the last character
     in the subpattern.  Set according to `count'.  */
  char *cutoff;

  /* true if we found a fast match (of patend) on the previous path.  */
  boolean prev_fast_match = false;
  /* The return value.  */
  int printed = 0;

  /* true if reading a bigram-encoded database.  */
  boolean old_format = false;
  /* For the old database format,
     the first and second characters of the most common bigrams.  */
  char bigram1[128], bigram2[128];

  /* To check the age of the database.  */
  struct stat st;
  time_t now;

  if (stat (dbfile, &st) || (fp = fopen (dbfile, "r")) == NULL)
    {
      error (0, errno, "%s", dbfile);
      return 0;
    }
  time(&now);
  if (now - st.st_mtime > WARN_SECONDS)
    {
      /* For example:
	 warning: database `fred' is more than 8 days old */
      error (0, 0, _("warning: database `%s' is more than %d %s old"),
	     dbfile, WARN_NUMBER_UNITS, _(warn_name_units));
    }

  pathsize = 1026;		/* Increased as necessary by locate_read_str.  */
  path = xmalloc (pathsize);

  nread = fread (path, 1, sizeof (LOCATEDB_MAGIC), fp);
  if (nread != sizeof (LOCATEDB_MAGIC)
      || memcmp (path, LOCATEDB_MAGIC, sizeof (LOCATEDB_MAGIC)))
    {
      int i;
      /* Read the list of the most common bigrams in the database.  */
      fseek (fp, 0, 0);
      for (i = 0; i < 128; i++)
	{
	  bigram1[i] = getc (fp);
	  bigram2[i] = getc (fp);
	}
      old_format = true;
    }

  /* If we ignore case,
     convert it to lower first so we don't have to do it every time */
  if (ignore_case){
    for (patend=pathpart;*patend;++patend){
     *patend=TOLOWER(*patend);
    }
  }
  
  
  globflag = strchr (pathpart, '*') || strchr (pathpart, '?')
    || strchr (pathpart, '[');

  patend = last_literal_end (pathpart);

  c = getc (fp);
  while (c != EOF)
    {
      register char *s;		/* Scan the path we read in.  */

      if (old_format)
	{
	  /* Get the offset in the path where this path info starts.  */
	  if (c == LOCATEDB_OLD_ESCAPE)
	    count += getw (fp) - LOCATEDB_OLD_OFFSET;
	  else
	    count += c - LOCATEDB_OLD_OFFSET;

	  /* Overlay the old path with the remainder of the new.  */
	  for (s = path + count; (c = getc (fp)) > LOCATEDB_OLD_ESCAPE;)
	    if (c < 0200)
	      *s++ = c;		/* An ordinary character.  */
	    else
	      {
		/* Bigram markers have the high bit set. */
		c &= 0177;
		*s++ = bigram1[c];
		*s++ = bigram2[c];
	      }
	  *s-- = '\0';
	}
      else
	{
	  if (c == LOCATEDB_ESCAPE)
	    count += (short)get_short (fp);
	  else if (c > 127)
	    count += c - 256;
	  else
	    count += c;

	  assert(count >= 0 && count <= last_pathlen);
	  
	  if (count > strlen(path))
	    {
	      /* This should not happen generally , but since we're
	       * reading in data which is outside our control, we
	       * cannot prevent it.
	       */
	      error(1, 0, _("locate database `%s' is corrupt or invalid"), dbfile);
	    }
	  
	  /* Overlay the old path with the remainder of the new.  */
	  nread = locate_read_str (&path, &pathsize, fp, 0, count); 
	  if (nread < 0)
	    break;
	  c = getc (fp);
	  s = path + count + nread - 1; /* Move to the last char in path.  */
	  assert (s[0] != '\0');
	  assert (s[1] == '\0'); /* Our terminator.  */
	  assert (s[2] == '\0'); /* Added by locate_read_str.  */
	}

      /* If the previous path matched, scan the whole path for the last
	 char in the subpattern.  If not, the shared prefix doesn't match
	 the pattern, so don't scan it for the last char.  */
      cutoff = prev_fast_match ? path : path + count;

      /* Search backward starting at the end of the path we just read in,
	 for the character at the end of the last glob-free subpattern
	 in PATHPART.  */
      for(prev_fast_match=false; s>=cutoff; s--)
        {
          char *s2;   /* Scan the path we read in. */
          register char *p2;  /* Scan `patend'.  */

          /* Fast first char check. */
          if(ignore_case)
            {
              if(TOLOWER(*s)!=*patend)
                continue;
            }
          else if(*s!=*patend)
            continue;

          if(ignore_case)
            for(s2=s-1, p2=patend-1; *p2!='\0' && TOLOWER(*s2)==*p2; s2--, p2--);
          else
            for(s2=s-1, p2=patend-1; *p2!='\0' && *s2==*p2; s2--, p2--);

          if(*p2!='\0')
            continue;
          /*  Success on the fast match.  Compare the whole pattern
              if it contains globbing characters.  */
          prev_fast_match = true;
          if(globflag)
            {
              if(ignore_case)
                {
                  if(fnmatch(pathpart,basename(path),FNM_CASEFOLD)!=0)
                    break;
                }
              else
                if(fnmatch(pathpart,basename(path),0)!=0)
                  break;
            }
          if(check_existence && stat(path,&st)!=0)
            break;
          puts(path);
          ++printed;
          break;
        }
    }
  
  if (ferror (fp))
    {
      error (0, errno, "%s", dbfile);
      return 0;
    }
  if (fclose (fp) == EOF)
    {
      error (0, errno, "%s", dbfile);
      return 0;
    }

  return printed;
}

#else

/* Print the entries in DBFILE that match shell globbing pattern PATHPART.
   Return the number of entries printed.  */

static unsigned long
new_locate (char *pathpart, char *dbfile, int ignore_case, int enable_print, int basename_only)
{
  /* The pathname database.  */
  FILE *fp;
  /* An input byte.  */
  int c;
  /* Number of bytes read from an entry.  */
  int nread;

  /* true if PATHPART contains globbing metacharacters.  */
  boolean globflag;
  /* The end of the last glob-free subpattern in PATHPART.  */
  char *patend;

  /* The current input database entry.  */
  char *path;
  /* Amount allocated for it.  */
  size_t pathsize;

  /* The length of the last path.  */
  size_t last_pathlen;

  /* The length of the prefix shared with the previous database entry.  */
  int count = 0;
  /* Where in `path' to stop the backward search for the last character
     in the subpattern.  Set according to `count'.  */
  char *cutoff;

  /* true if we found a fast match (of patend) on the previous path.  */
  boolean prev_fast_match = false;

  /* true if reading a bigram-encoded database.  */
  boolean old_format = false;
  /* For the old database format,
     the first and second characters of the most common bigrams.  */
  char bigram1[128], bigram2[128];

  /* number of items accepted (i.e. printed) */
  unsigned long int items_accepted = 0uL;

  /* To check the age of the database.  */
  struct stat st;
  time_t now;


  /* Set up the inspection regime */
  if (check_existence)
    add_visitor(visit_exists, NULL);

  if (strchr    (pathpart, '*')
      || strchr (pathpart, '?')
      || strchr (pathpart, '['))
    {
      if (ignore_case)
	add_visitor(visit_globmatch_casefold, pathpart);
      else
	add_visitor(visit_globmatch_nofold, pathpart);
    }
  else
    {
      /* No glob characters reuired.  Hence we match on 
       * _any part_ of the filename, not just the 
       * basename.  This seems odd to me, but it was 
       * the behaviour of the code before I touched it...
       * James Youngman <jay@gnu.org> 
       */
      add_visitor(visit_substring_match, pathpart);
    }

  if (enable_print)
    add_visitor(visit_justprint, NULL);
  
  if (stat (dbfile, &st) || (fp = fopen (dbfile, "r")) == NULL)
    {
      error (0, errno, "%s", dbfile);
      return 0;
    }
  time(&now);
  if (now - st.st_mtime > WARN_SECONDS)
    {
      /* For example:
	 warning: database `fred' is more than 8 days old */
      error (0, 0, _("warning: database `%s' is more than %d %s old"),
	     dbfile, WARN_NUMBER_UNITS, _(warn_name_units));
    }

  pathsize = 1026;		/* Increased as necessary by locate_read_str.  */
  path = xmalloc (pathsize);

  nread = fread (path, 1, sizeof (LOCATEDB_MAGIC), fp);
  if (nread != sizeof (LOCATEDB_MAGIC)
      || memcmp (path, LOCATEDB_MAGIC, sizeof (LOCATEDB_MAGIC)))
    {
      int i;
      /* Read the list of the most common bigrams in the database.  */
      fseek (fp, 0, 0);
      for (i = 0; i < 128; i++)
	{
	  bigram1[i] = getc (fp);
	  bigram2[i] = getc (fp);
	}
      old_format = true;
    }

  /* If we ignore case,
     convert it to lower first so we don't have to do it every time */
  if (ignore_case){
    for (patend=pathpart;*patend;++patend){
     *patend=TOLOWER(*patend);
    }
  }
  
  items_accepted = 0;
  
  globflag = strchr (pathpart, '*') || strchr (pathpart, '?')
    || strchr (pathpart, '[');

  patend = last_literal_end (pathpart);

  c = getc (fp);
  while (c != EOF)
    {
      register char *s;		/* Scan the path we read in.  */

      if (old_format)
	{
	  /* Get the offset in the path where this path info starts.  */
	  if (c == LOCATEDB_OLD_ESCAPE)
	    count += getw (fp) - LOCATEDB_OLD_OFFSET;
	  else
	    count += c - LOCATEDB_OLD_OFFSET;

	  /* Overlay the old path with the remainder of the new.  */
	  for (s = path + count; (c = getc (fp)) > LOCATEDB_OLD_ESCAPE;)
	    if (c < 0200)
	      *s++ = c;		/* An ordinary character.  */
	    else
	      {
		/* Bigram markers have the high bit set. */
		c &= 0177;
		*s++ = bigram1[c];
		*s++ = bigram2[c];
	      }
	  *s-- = '\0';
	}
      else
	{
	  if (c == LOCATEDB_ESCAPE)
	    count += (short)get_short (fp);
	  else if (c > 127)
	    count += c - 256;
	  else
	    count += c;

	  assert(count >= 0 && count <= last_pathlen);
	  
	  if (count > strlen(path))
	    {
	      /* This should not happen generally , but since we're
	       * reading in data which is outside our control, we
	       * cannot prevent it.
	       */
	      error(1, 0, _("locate database `%s' is corrupt or invalid"), dbfile);
	    }
	  
	  /* Overlay the old path with the remainder of the new.  */
	  nread = locate_read_str (&path, &pathsize, fp, 0, count); 
	  if (nread < 0)
	    break;
	  c = getc (fp);
	  s = path + count + nread - 1; /* Move to the last char in path.  */
	  assert (s[0] != '\0');
	  assert (s[1] == '\0'); /* Our terminator.  */
	  assert (s[2] == '\0'); /* Added by locate_read_str.  */
	}

      if (VISIT_ACCEPTED == process_filename(basename_only ? basename(path) : path))
	++items_accepted;
    }
  
  if (ferror (fp))
    {
      error (0, errno, "%s", dbfile);
      return 0;
    }
  if (fclose (fp) == EOF)
    {
      error (0, errno, "%s", dbfile);
      return 0;
    }

  return items_accepted;
}


#endif

extern char *version_string;

/* The name this program was run with. */
char *program_name;

static void
usage (stream)
     FILE *stream;
{
  fprintf (stream, _("\
Usage: %s [-d path | --database=path] [-e | --existing]\n\
      [-i | --ignore-case] [--wholepath] [--basename] [--version] [--help] pattern...\n"),
	   program_name);
  fputs (_("\nReport bugs to <bug-findutils@gnu.org>.\n"), stream);
}

static void
sanity_check_dbpath(const char *path)
{
  size_t len;
  
  if (':' == path[0])
    {
      error(0, 0, _("warning: locate database path `%s' contains a leading colon, which is not a valid database name"));
    }

  len = strlen(path);
  if (len > 0u)
    {
      if (':' == path[len-1u])
	{
	  error(0, 0, _("warning: locate database path `%s' contains a trailing colon, which is not a valid database name"));
	}
    }
}


static struct option const longopts[] =
{
  {"database", required_argument, NULL, 'd'},
  {"existing", no_argument, NULL, 'e'},
  {"ignore-case", no_argument, NULL, 'i'},
  {"help", no_argument, NULL, 'h'},
  {"version", no_argument, NULL, 'v'},
  {"null", no_argument, NULL, '0'},
  {"count", no_argument, NULL, 'c'},
  {"wholename", no_argument, NULL, 'w'},
  {"basename", no_argument, NULL, 'b'},
  {NULL, no_argument, NULL, 0}
};

int
main (argc, argv)
     int argc;
     char **argv;
{
  char *dbpath;
  unsigned long int found = 0uL;
  int optc;
  int ignore_case = 0;
  int print = 1;
  int just_count = 0;
  int basename_only = 0;
  
  program_name = argv[0];

#ifdef HAVE_SETLOCALE
  setlocale (LC_ALL, "");
#endif
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  dbpath = getenv ("LOCATE_PATH");
  if (dbpath == NULL)
    dbpath = LOCATE_DB;

  check_existence = 0;

  while ((optc = getopt_long (argc, argv, "cd:ei0", longopts, (int *) 0)) != -1)
    switch (optc)
      {
      case '0':
	separator = 0;
	break;

      case 'b':
	basename_only = 1;
	break;

      case 'c':
	just_count = 1;
	print = 0;
	break;

      case 'd':
	dbpath = optarg;
	break;

      case 'e':
	check_existence = 1;
	break;

      case 'i':
	ignore_case = 1;
	break;
	
      case 'h':
	usage (stdout);
	return 0;

      case 'v':
	printf (_("GNU locate version %s\n"), version_string);
	return 0;

      case 'w':
	basename_only = 0;
	break;

      default:
	usage (stderr);
	return 1;
      }

  if (optind == argc)
    {
      usage (stderr);
      return 1;
    }

  sanity_check_dbpath(dbpath);

  for (; optind < argc; optind++)
    {
      char *e;
      next_element (dbpath);	/* Initialize.  */
      while ((e = next_element ((char *) NULL)) != NULL)
	{
	  found += new_locate (argv[optind], e, ignore_case, print, basename_only);
	}
    }

  if (just_count)
    {
      printf("%ld\n", found);
    }
  
  if (found)
    return 0;
  else
    return 1;
}

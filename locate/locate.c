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
   Modified by David MacKenzie <djm@gnu.org>.  */

#include <config.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fnmatch.h>
#include <getopt.h>
#include <xstrtol.h>

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
#include "../gnulib/lib/human.h"
#include "dirname.h"
#include "closeout.h"
#include "nextelem.h"
#include "regex.h"


/* Note that this evaluates C many times.  */
#ifdef _LIBC
# define TOUPPER(Ch) toupper (Ch)
# define TOLOWER(Ch) tolower (Ch)
#else
# define TOUPPER(Ch) (islower (Ch) ? toupper (Ch) : (Ch))
# define TOLOWER(Ch) (isupper (Ch) ? tolower (Ch) : (Ch))
#endif

/* typedef enum {false, true} boolean; */

/* Warn if a database is older than this.  8 days allows for a weekly
   update that takes up to a day to perform.  */
#define WARN_NUMBER_UNITS (8)
/* Printable name of units used in WARN_SECONDS */
static const char warn_name_units[] = N_("days");
#define SECONDS_PER_UNIT (60 * 60 * 24)

#define WARN_SECONDS ((SECONDS_PER_UNIT) * (WARN_NUMBER_UNITS))

/* Check for existence of files before printing them out? */
static int check_existence = 0;

static int follow_symlinks = 1;

/* What to separate the results with. */
static int separator = '\n';




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

const char * const metacharacters = "*?[]\\";

/* Return nonzero if S contains any shell glob characters.
 */
static int 
contains_metacharacter(const char *s)
{
  if (NULL == strpbrk(s, metacharacters))
    return 0;
  else
    return 1;
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


static void
lc_strcpy(char *dest, const char *src)
{
  while (*src)
    {
      *dest++ = TOLOWER(*src);
      ++src;
    }
  *dest = 0;
}

enum visit_result
  {
    VISIT_CONTINUE = 1,  /* please call the next visitor */
    VISIT_ACCEPTED = 2,  /* accepted, call no futher callbacks for this file */
    VISIT_REJECTED = 4,  /* rejected, process next file. */
    VISIT_ABORT    = 8   /* rejected, process no more files. */
  };


struct locate_stats
{
  uintmax_t compressed_bytes;
  uintmax_t total_filename_count;
  uintmax_t total_filename_length;
  uintmax_t whitespace_count;
  uintmax_t newline_count;
  uintmax_t highbit_filename_count;
};
static struct locate_stats statistics;


struct casefolder
{
  const char *pattern;
  char *buffer;
  size_t buffersize;
};

struct regular_expression
{
  regex_t re;
};



typedef int (*visitfunc)(const char *munged_filename,
			 const char *original_filename,
			 void *context);

struct visitor
{
  visitfunc      inspector;
  void *         context;
  struct visitor *next;
};


static struct visitor *inspectors = NULL;
static struct visitor *lastinspector = NULL;

static int
process_filename(const char *munged_filename, const char *original_filename)
{
  int result = VISIT_CONTINUE;
  const struct visitor *p = inspectors;
  
  while ( (VISIT_CONTINUE == result) && (NULL != p) )
    {
      result = (p->inspector)(munged_filename, original_filename, p->context);
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
      lastinspector = p;
    }
}



static int
visit_justprint(const char *munged_filename, const char *original_filename, void *context)
{
  (void) context;
  (void) munged_filename;
  fputs(original_filename, stdout);
  putchar(separator);
  return VISIT_CONTINUE;
}

static int
visit_exists_follow(const char *munged_filename,
		    const char *original_filename, void *context)
{
  struct stat st;
  (void) context;
  (void) munged_filename;

  /* munged_filename has been converted in some way (to lower case,
   * or is just the base name of the file), and original_filename has not.  
   * Hence only original_filename is still actually the name of the file 
   * whose existence we would need to check.
   */
  if (stat(original_filename, &st) != 0)
    {
      return VISIT_REJECTED;
    }
  else
    {
      return VISIT_CONTINUE;
    }
}

static int
visit_exists_nofollow(const char *munged_filename,
		      const char *original_filename, void *context)
{
  struct stat st;
  (void) context;
  (void) munged_filename;

  /* munged_filename has been converted in some way (to lower case,
   * or is just the base name of the file), and original_filename has not.  
   * Hence only original_filename is still actually the name of the file 
   * whose existence we would need to check.
   */
  if (lstat(original_filename, &st) != 0)
    {
      return VISIT_REJECTED;
    }
  else
    {
      return VISIT_CONTINUE;
    }
}

static int
visit_substring_match_nocasefold(const char *munged_filename, const char *original_filename, void *context)
{
  const char *pattern = context;
  (void) original_filename;

  if (NULL != strstr(munged_filename, pattern))
    return VISIT_CONTINUE;
  else
    return VISIT_REJECTED;
}

static int
visit_substring_match_casefold(const char *munged_filename, const char *original_filename, void *context)
{
  struct casefolder * p = context;
  size_t len = strlen(munged_filename);

  (void) original_filename;
  if (len+1 > p->buffersize)
    {
      p->buffer = xrealloc(p->buffer, len+1); /* XXX: consider using extendbuf(). */
      p->buffersize = len+1;
    }
  lc_strcpy(p->buffer, munged_filename);
  
  
  if (NULL != strstr(p->buffer, p->pattern))
    return VISIT_CONTINUE;
  else
    return VISIT_REJECTED;
}


static int
visit_globmatch_nofold(const char *munged_filename, const char *original_filename, void *context)
{
  const char *glob = context;
  (void) original_filename;
  if (fnmatch(glob, munged_filename, 0) != 0)
    return VISIT_REJECTED;
  else
    return VISIT_CONTINUE;
}


static int
visit_globmatch_casefold(const char *munged_filename, const char *original_filename, void *context)
{
  const char *glob = context;
  (void) original_filename;
  if (fnmatch(glob, munged_filename, FNM_CASEFOLD) != 0)
    return VISIT_REJECTED;
  else
    return VISIT_CONTINUE;
}


static int
visit_regex(const char *munged_filename, const char *original_filename, void *context)
{
  struct regular_expression *p = context;

  if (0 == regexec(&p->re, munged_filename, 0u, NULL, 0))
    return VISIT_CONTINUE;	/* match */
  else
    return VISIT_REJECTED;	/* no match */
}


static int
visit_stats(const char *munged_filename, const char *original_filename, void *context)
{
  struct locate_stats *p = context;
  size_t len = strlen(original_filename);
  const char *s;
  int highbit, whitespace, newline;

  ++(p->total_filename_count);
  p->total_filename_length += len;
  
  highbit = whitespace = newline = 0;
  for (s=original_filename; *s; ++s)
    {
      if ( (int)(*s) & 128 )
	highbit = 1;
      if ('\n' == *s)
	{
	  newline = whitespace = 1;
	}
      else if (isspace((unsigned char)*s))
	{
	  whitespace = 1;
	}
    }

  if (highbit)
    ++(p->highbit_filename_count);
  if (whitespace)
    ++(p->whitespace_count);
  if (newline)
    ++(p->newline_count);

  return VISIT_CONTINUE;
}


/* Emit the statistics.
 */
static void
print_stats(size_t database_file_size)
{
  char hbuf[LONGEST_HUMAN_READABLE + 1];
  
  printf(_("Locate database size: %s bytes\n"),
	 human_readable ((uintmax_t) database_file_size,
			 hbuf, human_ceiling, 1, 1));
  
  printf(_("Filenames: %s "),
	 human_readable (statistics.total_filename_count,
			 hbuf, human_ceiling, 1, 1));
  printf(_("with a cumulative length of %s bytes"),
	 human_readable (statistics.total_filename_length,
			 hbuf, human_ceiling, 1, 1));
  
  printf(_("\n\tof which %s contain whitespace, "),
	 human_readable (statistics.whitespace_count,
			 hbuf, human_ceiling, 1, 1));
  printf(_("\n\t%s contain newline characters, "),
	 human_readable (statistics.newline_count,
			 hbuf, human_ceiling, 1, 1));
  printf(_("\n\tand %s contain characters with the high bit set.\n"),
	 human_readable (statistics.highbit_filename_count,
			 hbuf, human_ceiling, 1, 1));
  
  printf(_("Compression ratio %4.2f%%\n"),
	 100.0 * ((double)statistics.total_filename_length
		  - (double) database_file_size)
	 / (double) statistics.total_filename_length);
  printf("\n");
}


/* Print the entries in DBFILE that match shell globbing pattern PATHPART.
   Return the number of entries printed.  */

static unsigned long
new_locate (char *pathpart,
	    char *dbfile,
	    int ignore_case,
	    int enable_print,
	    int basename_only,
	    int use_limit,
	    uintmax_t limit,
	    int stats,
	    int regex)
{
  FILE *fp;			/* The pathname database.  */
  int c;			/* An input byte.  */
  int nread;		     /* number of bytes read from an entry. */
  char *path;		       /* The current input database entry. */
  const char *testpath;
  size_t pathsize;		/* Amount allocated for it.  */
  int count = 0; /* The length of the prefix shared with the previous database entry.  */
  
  int old_format = 0; /* true if reading a bigram-encoded database.  */
  
  /* for the old database format,
     the first and second characters of the most common bigrams.  */
  char bigram1[128], bigram2[128];

  /* number of items accepted (i.e. printed) */
  unsigned long int items_accepted = 0uL;

  /* To check the age of the database.  */
  struct stat st;
  time_t now;

  /* Set up the inspection regime */
  inspectors = NULL;
  lastinspector = NULL;

  if (stats)
    {
      assert(!use_limit);
      add_visitor(visit_stats, &statistics);
    }
  else
    {
      if (regex)
	{
	  struct regular_expression *p = xmalloc(sizeof(*p));
	  int cflags = REG_EXTENDED | REG_NOSUB 
	    | (ignore_case ? REG_ICASE : 0);
	  errno = 0;
	  if (0 == regcomp(&p->re, pathpart, cflags))
	    {
	      add_visitor(visit_regex, p);
	    }
	  else 
	    {
	      error (1, errno, "Invalid regular expression; %s", pathpart);
	    }
	}
      else if (contains_metacharacter(pathpart))
	{
	  if (ignore_case)
	    add_visitor(visit_globmatch_casefold, pathpart);
	  else
	    add_visitor(visit_globmatch_nofold, pathpart);
	}
      else
	{
	  /* No glob characters used.  Hence we match on 
	   * _any part_ of the filename, not just the 
	   * basename.  This seems odd to me, but it is the 
	   * traditional behaviour.
	   * James Youngman <jay@gnu.org> 
	   */
	  if (ignore_case)
	    {
	      struct casefolder * cf = xmalloc(sizeof(*cf));
	      cf->pattern = pathpart;
	      cf->buffer = NULL;
	      cf->buffersize = 0;
	      add_visitor(visit_substring_match_casefold, cf);
	    }
	  else
	    {
	      add_visitor(visit_substring_match_nocasefold, pathpart);
	    }
	}

      /* We add visit_exists_*() as late as possible to reduce the
       * number of stat() calls.
       */
      if (check_existence)
	{
	  visitfunc f;
	  if (follow_symlinks)
	    f = visit_exists_follow;
	  else
	    f = visit_exists_nofollow;
	  
	  add_visitor(f, NULL);
	}
      

      if (enable_print)
	add_visitor(visit_justprint, NULL);
    }
  

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
      old_format = 1;
    }

  if (stats)
    {
	printf(_("Database %s is in the %s format.\n"),
	       dbfile,
	       old_format ? _("old") : "LOCATE02");
    }
  
  /* If we ignore case, convert it to lower first so we don't have to
   * do it every time
   */
  if (!stats && ignore_case)
    {
      lc_strcpy(pathpart, pathpart);
    }
  
  items_accepted = 0;

  c = getc (fp);
  while ( (c != EOF) && (!use_limit || (limit > 0)) )
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

      testpath = basename_only ? base_name(path) : path;
      if (VISIT_ACCEPTED == process_filename(testpath, path))
	{
	  if ((++items_accepted >= limit) && use_limit)
	    {
	      break;
	    }
	}
    }

      
  if (stats)
    {
      print_stats(st.st_size);
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




extern char *version_string;

/* The name this program was run with. */
char *program_name;

static void
usage (stream)
     FILE *stream;
{
  fprintf (stream, _("\
Usage: %s [-d path | --database=path] [-e | --existing]\n\
      [-i | --ignore-case] [-w | --wholename] [-b | --basename] \n\
      [--limit=N | -l N] [-S | --statistics] [-0 | --null] [-c | --count]\n\
      [-P | -H | --nofollow] [-L | --follow] [-m | --mmap ] [ -s | --stdio ]\n\
      [-r | --regex ] [--version] [--help] pattern...\n"),
	   program_name);
  fputs (_("\nReport bugs to <bug-findutils@gnu.org>.\n"), stream);
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
  {"wholepath", no_argument, NULL, 'w'}, /* Synonym. */
  {"basename", no_argument, NULL, 'b'},
  {"stdio", no_argument, NULL, 's'},
  {"mmap",  no_argument, NULL, 'm'},
  {"limit",  required_argument, NULL, 'l'},
  {"regex",  no_argument, NULL, 'r'},
  {"statistics",  no_argument, NULL, 'S'},
  {"follow",      no_argument, NULL, 'L'},
  {"nofollow",    no_argument, NULL, 'P'},
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
  uintmax_t limit = 0;
  int use_limit = 0;
  int regex = 0;
  int stats = 0;
  
  program_name = argv[0];

#ifdef HAVE_SETLOCALE
  setlocale (LC_ALL, "");
#endif
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
  atexit (close_stdout);

  dbpath = getenv ("LOCATE_PATH");
  if (dbpath == NULL)
    dbpath = LOCATE_DB;

  check_existence = 0;

  while ((optc = getopt_long (argc, argv, "bcd:eil:rsm0SwHPL", longopts, (int *) 0)) != -1)
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

      case 'r':
	regex = 1;
	break;

      case 'S':
	stats = 1;
	break;

      case 'L':
	follow_symlinks = 1;
	break;

	/* In find, -P and -H differ in the way they handle paths
	 * given on the command line.  This is not relevant for
	 * locate, but the -H option is supported because it is
	 * probably more intuitive to do so.
	 */
      case 'P':
      case 'H':
	follow_symlinks = 0;
	break;

      case 'l':
	{
	  char *end = optarg;
	  strtol_error err = xstrtoumax(optarg, &end, 10, &limit, NULL);
	  if (LONGINT_OK != err)
	    {
	      STRTOL_FATAL_ERROR(optarg, _("argument to --limit"), err);
	    }
	  use_limit = 1;
	}
	break;

      case 's':			/* use stdio */
      case 'm':			/* use mmap  */
	/* These options are implemented simply for
	 * compatibility with FreeBSD
	 */ 
	break;

      default:
	usage (stderr);
	return 1;
      }

  if (stats)
    {
      use_limit = 0;
      print = 0;
    }
  else
    {
      if (optind == argc)
	{
	  usage (stderr);
	  return 1;
	}
    }
  
  for (; stats || optind < argc; optind++)
    {
      char *e;
      const char *needle;
      next_element (dbpath, 0);	/* Initialize.  */
      needle = stats ? NULL : argv[optind];
      while ((e = next_element ((char *) NULL, 0)) != NULL)
	{
	  statistics.compressed_bytes = 
	    statistics.total_filename_count = 
	    statistics.total_filename_length = 
	    statistics.whitespace_count = 
	    statistics.newline_count = 
	    statistics.highbit_filename_count = 0u;

	  if (0 == strlen(e) || 0 == strcmp(e, "."))
	    {
	      /* Use the default database name instead (note: we
	       * don't use 'dbpath' since that might itself contain a 
	       * colon-separated list.
	       */
	      e = LOCATE_DB;
	    }
	  
	  found += new_locate (needle, e, ignore_case, print, basename_only, use_limit, limit, stats, regex);
	}
      if (stats)
	break;
    }

  if (just_count)
    {
      printf("%ld\n", found);
    }
  
  if (found || (use_limit && (limit==0)) || stats )
    return 0;
  else
    return 1;
}

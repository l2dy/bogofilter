/* $Id$ */

#include "common.h"

#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

#include "bogofilter.h"
#include "bogohome.h"
#include "datastore.h"
#include "msgcounts.h"
#include "paths.h"
#include "rand_sleep.h"
#include "wordlists.h"
#include "xmalloc.h"
#include "xstrdup.h"

#include "bsdqueue.h"

#define	MIN_SLEEP	0.5e+3		/* 0.5 milliseconds */
#define	MAX_SLEEP	2.0e+6		/* 2.0 seconds */

#ifndef	RAND_MAX			/* SunOS 4.1.X needs this */
#define	RAND_MAX	2147483647	/* May not work on SysV   */
#endif

/* Function Definitions */

#define MAX_ENVS    10

/* Idea: - build dirname
 *	 - search list of known environments if we have that dirname,
 *	   if so, return it
 *	 - create and insert new environment
 *
 * for destruction: - when closed an environment, NULL it in word_lists
 * past the current one that match our address
 */

/** returns malloc()ed copy of the directory name of \a path,
 * or bogohome if no directory is part of \a path.
 */
static char *bf_dirname(const char *path) {
    char *x = xstrdup(path), *t;
    t = strrchr(x, DIRSEP_C);
    if (t) *t = '\0';
    else xfree(x), x = xstrdup(bogohome);
    return x;
}

static LIST_HEAD(envlist, envnode) envs;
struct envlist envlisthead;
struct envnode {
    LIST_ENTRY(envnode) entries;
    void *dbe;
    char directory[1];
};

static void *list_searchinsert(const char *directory) {
    struct envnode *i, *n;

    assert(directory);

    for (i = envlisthead.lh_first ; i ; i = i->entries.le_next) {
	if (strcmp(directory, &i->directory[0]) == 0)
	    return i->dbe;
    }

    n = xmalloc(sizeof(struct envnode) + strlen(directory) + 1);
    n->dbe = ds_init(directory);
    ds_txn_begin(n->dbe);
    strcpy(&n->directory[0], directory);
    LIST_INSERT_HEAD(&envlisthead, n, entries);
    return n->dbe;
}

static bool open_wordlist(wordlist_t *list, dbmode_t mode)
{
    bool retry = false;
    void *dbe;

    if (list->type == WL_IGNORE) 	/* open ignore list in read-only mode */
	mode = DS_READ;

    /* FIXME: create or reuse environment from filepath */

    dbe = list_searchinsert(bf_dirname(list->filepath));
    if (dbe == NULL)
	exit(EX_ERROR);
    list->dsh = ds_open(dbe, bogohome, list->filepath, mode); /* FIXME */
    if (list->dsh == NULL) {
	int err = errno;
	close_wordlists(word_lists, false); /* unlock and close */
	switch(err) {
	    /* F_SETLK can't obtain lock */
	case EAGAIN:
#ifdef __EMX__
	case EACCES:
#endif
	    /* case EACCES: */
	    rand_sleep(MIN_SLEEP, MAX_SLEEP);
	    retry = true;
	    break;
	default:
	    if (query)	/* If "-Q", failure is OK */
		return false;
	    fprintf(stderr,
		    "Can't open file '%s' in directory '%s'.\n",
		    list->filepath, bogohome);
	    if (err != 0)
		fprintf(stderr,
			"error #%d - %s.\n", err, strerror(err));
	    if (err == ENOENT)
		fprintf(stderr, 
			"\n"
			"Remember to register some spam and ham messages before you\n"
			"use bogofilter to evaluate mail for its probable spam status!\n");
	    if (err == EINVAL)
		fprintf(stderr,
			"\n"
			"Make sure that the BerkeleyDB version this program is linked against\n"
			"can handle the format of the data base file (after updates in particular)\n"
			"and that your NFS locking, if applicable, works.\n");
	    exit(EX_ERROR);
	} /* switch */
    } else { /* ds_open */
	dsv_t val;

	switch (ds_get_msgcounts(list->dsh, &val)) {
	    case 0:
	    case 1:
		list->msgcount[IX_GOOD] = val.goodcount;
		list->msgcount[IX_SPAM] = val.spamcount;
		if (wordlist_version == 0 &&
			ds_get_wordlist_version(list->dsh, &val) == 0)
		    wordlist_version = val.count[0];
		break;
	    case DS_ABORT_RETRY:
		retry = true;
		break;
	}
    }

    return retry;
}

void open_wordlists(wordlist_t *list, dbmode_t mode)
{
    bool retry = true;

    /* set bogohome using first wordlist's directory */
    set_wordlist_directory();

    LIST_INIT(&envs);

    while (retry) {
	if (run_type & (REG_SPAM | REG_GOOD | UNREG_SPAM | UNREG_GOOD))
	    retry = open_wordlist(get_default_wordlist(list), mode);
	else {
	    wordlist_t *i;
	    retry = false;
	    for (i = list; i ; i = i->next) {
		retry |= open_wordlist(i, i->type == WL_IGNORE ? DS_READ : mode);
	    }  /* for */
	}
    }
}

/* set_wordlist_directory()
**	set bogohome using first wordlist's directory
**	if none, don't change bogohome
*/

void set_wordlist_directory(void)
{
    if (word_lists != NULL) {
	char *path = get_directory_from_path(word_lists->filepath);
	if (path != NULL)
	    set_bogohome(path);
    }
}

/** close all open word lists */
bool close_wordlists(wordlist_t *list, bool commit /** if unset, abort */)
    /* FIXME: we really need to look at the list's environments */
{
    bool err = false;
    struct envnode *i;

    for (i = envlisthead.lh_first; i; i = i->entries.le_next) {
	if (i->dbe) {
	    if (commit) {
		if (ds_txn_commit(i->dbe)) {
		    err = true;
		}
	    } else {
		ds_txn_abort(i->dbe);
	    }
	}
    }

    for (; list ; list = list->next) {
	if (list->dsh) {
	    ds_close(list->dsh);
	    list->dsh = NULL;
	}
    }

    while ((i = envlisthead.lh_first)) {
	ds_cleanup(i->dbe);
	LIST_REMOVE(i, entries);
	free(i);
    }
    return err;
}

bool build_wordlist_path(char *filepath, size_t size, const char *path)
{
    bool ok = build_path(filepath, size, path, WORDLIST);
    return ok;
}

#ifdef COMPILE_DEAD_CODE
/* some sanity checking of lists is needed because we may
   allow users to specify lists eventually and there are
   opportunities to generate divide-by-zero exceptions or
   follow bogus pointers. */
static void sanitycheck_lists(void)
{
    wordlist_t* list=word_lists;
    int listcount=0;

    while(1) {
	if (list == NULL) break;
	if (list->name == NULL) {
	    fprintf(stderr, "A list has no name.\n");
	    exit(EX_ERROR);
	}
	if (list->msgcount==0) {
	    fprintf(stderr, "list %s has zero message count.\n", list->name);
	    exit(EX_ERROR);
	}
	listcount++;
	list=list->next;
    }
    if (0==listcount) {
	fprintf(stderr, "No wordlists available!\n");
	exit(EX_ERROR);
    }
    if (DEBUG_WORDLIST(1))
	fprintf(dbgout, "%d lists look OK.\n", listcount);
}
#endif

static char *spanword(char *p)
{
    const char *delim = ", \t";
    p += strcspn(p, delim);		/* skip to end of word */
    *p++ = '\0';
    while (isspace((unsigned char)*p)) 	/* skip trailing whitespace */
	p += 1;
    return p;
}

/* type - 'n', or 'i' (normal or ignore)
 * name - 'user', 'system', 'ignore', etc
 * path - 'wordlist.db', 'ignorelist.db', etc
 * override - 1,2,...
 */

/* returns true for success, false for error */
bool configure_wordlist(const char *val)
{
    char  ch;
    WL_TYPE type;
    char* listname;
    char* filename;
    int  precedence;

    char *tmp = xstrdup(val);
    
    ch= tmp[0];		/* save wordlist type (good/spam) */
    tmp = spanword(tmp);
    
    switch (toupper(ch))
    {
    case 'R':
	type = WL_REGULAR;
	break;
    case 'I':
	type = WL_IGNORE;
	break;
    default:
	fprintf( stderr, "Unknown wordlist type - '%c'\n", ch);
	return (false);
    }
    
    listname=tmp;		/* name of wordlist */
    tmp = spanword(tmp);
    
    filename=tmp;		/* path to wordlist */
    tmp = spanword(tmp);
    
    precedence=atoi(tmp);
    tmp = spanword(tmp);
    
    init_wordlist(listname, filename, precedence, type);
    
    config_setup = true;

    return true;
}

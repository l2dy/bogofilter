/* $Id$ */
/* 
 * $Log$
 * Revision 1.12  2002/10/04 18:08:26  relson
 * Added a '-u' (update) option so that the appropriate wordlist, i.e. spamlist.db
 * or goodlist.db,  is updated after classifying the message.
 *
 * Revision 1.11  2002/10/02 16:27:40  relson
 * Initial inclusion of multiple wordlist code into bogofilter.
 *
 * Revision 1.10  2002/09/29 03:40:54  gyepi
 *
 * Modified: bogofilter.c bogofilter.h main.c
 * 1. replace Judy with hash table (wordhash)
 * 2. ensure that databases are always locked in the same order.
 *
 * Apologies for simultaneously submitting loosely related changes.
 *
 * Revision 1.9  2002/09/27 01:17:38  gyepi
 * removed unused bogodump declaration
 *
 * Revision 1.8  2002/09/26 23:04:41  relson
 * documentation:
 *     changed to refer to "good" and "spam" tokens and lists.
 *     removed '-l' option as this function is now in bogoutil.
 *
 * filenames:
 *     changed database from "hamlist.db" to "goodlist.db".
 *
 * variables:
 *     renamed "ham_list" and "hamness" to "good_list" and "goodness".
 *
 * Revision 1.7  2002/09/26 17:22:01  relson
 * Remove unused function prototypes.
 *
 * Revision 1.6  2002/09/24 04:34:19  gyepi
 *
 *
 *  Modified Files:
 *  	Makefile.am  -- add entries for datastore* + and other new files
 *         bogofilter.c bogofilter.h main.c -- fixup to use database abstraction
 *
 *  Added Files:
 *  	datastore_db.c datastore_db.h datastore.h -- database abstraction. Also implements locking
 * 	xmalloc.c xmalloc.h -- utility
 *  	bogoutil.c  -- dump/restore utility.
 *
 * 1. Implements database abstraction as discussed.
 *    Also implements multiple readers/single writer file locking.
 *
 * 2. Adds utility to dump/restore databases.
 *
 * Revision 1.5  2002/09/23 11:31:53  m-a
 * Unnest comments, and move $ line down by one to prevent CVS from adding nested comments again.
 *
 * Revision 1.4  2002/09/23 10:08:49  m-a
 * Integrate patch by Zeph Hull and Clint Adams to present spamicity in
 * X-Spam-Status header in bogofilter -p mode.
 *
 * Revision 1.3  2002/09/18 22:30:22  relson
 * Created lexer.h with the definitions needed by lexer_l.l from bogofilter.h.
 * This removes the compile-time dependency between the two files.
 *
 * Revision 1.2  2002/09/15 19:07:12  relson
 * Add an enumerated type for return codes of RC_SPAM and RC_NONSPAM, which  values of 0 and 1 as called for by procmail.
 * Use the new codes and type for bogofilter() and when generating the X-Spam-Status message.
 *
 * Revision 1.1.1.1  2002/09/14 22:15:20  adrian_otto
 * 0.7.3 Base Source
 * */
/*  constants and declarations for bogofilter */

#ifndef	HAVE_BOGOFILTER_H
#define	HAVE_BOGOFILTER_H

#include <lexer.h>
#include <wordlists.h>

#define GOOD_BIAS	2		// give good words more weight

typedef enum rc_e {RC_SPAM=0, RC_NONSPAM=1}  rc_t;
typedef enum reg_e { REG_NONE = 0, REG_SPAM, REG_GOOD, REG_SPAM_TO_GOOD, REG_GOOD_TO_SPAM } reg_t;

//Represents the secondary data for a word key
typedef struct {
  int freq;
  int msg_freq;
} wordprop_t;

extern int verbose, update;

extern void register_messages(int fd, reg_t register_type);
extern rc_t bogofilter(int fd, double *xss);

#endif	/* HAVE_BOGOFILTER_H */

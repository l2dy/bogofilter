/* $Id$ */

/*****************************************************************************

NAME:
   configfile.h -- prototypes and definitions for bogoconfig.c.

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#ifndef CONFIGFILE_H
#define CONFIGFILE_H

/* Definitions */

typedef enum {
	CP_NONE,
	CP_BOOLEAN,
	CP_INTEGER,
	CP_DOUBLE,
	CP_CHAR,
	CP_STRING,
	CP_DIRECTORY,
	CP_FUNCTION
} parm_t;

typedef bool func(const unsigned char *s);

typedef struct {
    const char *name;
    parm_t	type;
    union
    {
	void	*v;
	func	*f;
	bool	*b;
	int	*i;
	double	*d;
	char	*c;
	char   **s;
    } addr;
} parm_desc;

/* Global variables */

extern const char *logtag;
extern const char *spam_header_name;
extern const char *user_config_file;

extern const parm_desc sys_parms[];
extern const parm_desc *usr_parms;

void process_config_files(void);
void read_config_file(const char *fname, bool tilde_expand);

#endif

/* $Id$ */

#ifndef BOGOCONFIG_H
#define BOGOCONFIG_H

/* Definitions */

typedef enum {
	CP_NONE,
	CP_ALGORITHM,
	CP_BOOLEAN,
	CP_INTEGER,
	CP_DOUBLE,
	CP_CHAR,
	CP_STRING,
	CP_WORDLIST
} parm_t;

typedef struct {
    const char *name;
    parm_t	type;
    union
    {
	void	*v;
	bool	*b;
	int	*i;
	double	*d;
	char	*c;
	const char **s;
    } addr;
} parm_desc;

/* Global variables */

extern const char *spam_header_name;
extern const char *user_config_file;
int process_args(int argc, char **argv);
void process_config_files(void);

#endif

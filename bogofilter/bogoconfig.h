/* $Id$ */

#ifndef BOGOCONFIG_H
#define BOGOCONFIG_H

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
int process_args(int argc, char **argv);
void process_config_files(void);

#endif

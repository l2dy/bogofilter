/* $Id$ */

#ifndef BOGOCONFIG_H
#define BOGOCONFIG_H

/* Global variables */

extern const char *spam_header_name;
extern const char *user_config_file;
int process_args(int argc, char **argv);
void process_config_files(void);

#endif

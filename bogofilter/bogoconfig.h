#ifndef BOGOCONFIG_H
#define BOGOCONFIG_H
// Global variables

extern char *spam_header_name;
extern char *user_config_file;
void read_config_file(const char *filename, bool home_dir);

#endif

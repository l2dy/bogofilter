/*
David Relson wrote:

> Use these commands:

> 1:  bogoutil -d /your/path/wordlist.db > wordlist.txt
> 2:  mv /your/path/wordlist.db /your/path/wordlist.db.old
> 3:  install new bogofilter
> 4:  bogoutil -l /your/path/wordlist.db < wordlist.txt

I hope I have simplified this a bit. I wrote the attached small
converter which will convert any Depot database in a Villa database as
long as the compare function of the Villa database is this cmpkey()
function. In particular, Bogofilter's database can be converted to the
new format even *after* you installed version 0.93.0.

I hereby put it under the GNU GPL and you can include it together with
some notes in the 0.93.0 release (after some testing ;-)

-- 
Stefan Bellon

Content-Type: text/plain; Name="qdbm_dp2vl.c"
*/
#include <stdio.h>
#include <string.h>

#include <depot.h>
#include <cabin.h>
#include <villa.h>
#include <stdlib.h>

int cmpkey(const char *aptr, int asiz, const char *bptr, int bsiz)
{
    int aiter, biter;
    
    for (aiter = 0, biter = 0; aiter < asiz && biter < bsiz; ++aiter, ++biter) {
        if (aptr[aiter] != bptr[biter])
            return (aptr[aiter] < bptr[biter]) ? -1 : 1;
    }
    
    if (aiter == asiz && biter == bsiz)
        return 0;
    
    return (aiter == asiz) ? -1 : 1;
}

int main(int argc, char *argv[])
{
    DEPOT *dho;
    VILLA *dhn;
    char *new_name;

    int ret;
    int ksiz, dsiz;
    char *key, *data;

    if (argc != 2) {
        fprintf(stderr, "Usage:  qdbm_dp2vl database\n");
        exit(1);
    }
    
    dho = dpopen(argv[1], DP_OREADER, 0);
    if (!dho) {
        fprintf(stderr, "Couldn't open database '%s'.\n", argv[1]);
        exit(1);
    }
    
    new_name = malloc(strlen(argv[1])+4);
    if (!new_name) {
        fprintf(stderr, "Couldn't allocate memory.\n");
        dpclose(dho);
        exit(1);
    }
    
    new_name = strcpy(new_name, argv[1]);
    new_name = strcat(new_name, "-new");

    dhn = vlopen(new_name, VL_OWRITER | VL_OCREAT, cmpkey);
    if (!dhn) {
        fprintf(stderr, "Couldn't create database '%s'.\n", new_name);
        free(new_name);
        dpclose(dho);
        exit(1);
    }
    
    ret = dpiterinit(dho);
    if (ret) {
        while ((key = dpiternext(dho, &ksiz))) {
            data = dpget(dho, key, ksiz, 0, -1, &dsiz);
            if (data) {
                ret = vlput(dhn, key, ksiz, data, dsiz, VL_DOVER);
                if (!ret) {
                    fprintf(stderr, "Error writing key '%.*s', value '%.*s'\n",
                            ksiz, key, dsiz, data);
                }
                free(data);
            }
            free(key);
        }
    } else {
        fprintf(stderr, "Error creating database iterator.\n");
        free(new_name);
        dpclose(dho);
        vlclose(dhn);
        exit(1);
    }
    
    dpclose(dho);
    vlclose(dhn);
    
    fprintf(stderr, "Done.\n");

    return 0;
}

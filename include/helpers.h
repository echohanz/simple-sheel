// A header file for helpers.c
// Declare any additional functions in this file

#ifndef HELPERS_H_
#define HELPERS_H_ 
#include "linkedList.h"
#include "icssh.h"

void cd(char *path);
void estatus(int exit_status);
List_t *new_bgprocs();
void add_bgprocs(List_t *bgprocs, job_info* job, pid_t pid);
void exit_bgprocs(List_t *bgprocs);

bgentry_t* remove_bgprocs(List_t *bgprocs, pid_t pid);
void sigchild_handler(int sig);
void reap_bgprocs(List_t *bgprocs);
void bglist(List_t *bgprocs);

void siguser2_handler(int sig);

int run_procs(proc_info* proc, int infd, int outfd);

int check_redirect(proc_info* proc);

#endif /* ifndef HELPERS_H_ */

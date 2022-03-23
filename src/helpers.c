// Your helper functions need to be here.

#include "helpers.h"
#include "icssh.h"
#include "linkedList.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>

void cd(char *path) {
    if (path == NULL) {
        path = getenv("HOME");
    }
    if (chdir(path) < 0) {
        fprintf(stderr, DIR_ERR);
        return;
    }
    char buf[PATH_MAX];
    getcwd(buf, PATH_MAX);
    printf("%s\n", buf);
}

void estatus(int exit_status) {
    printf("%d\n", WEXITSTATUS(exit_status));
}

static int cmp_bgentry(void *a, void *b) {
   bgentry_t *abg = a;
   bgentry_t *bbg = b;
   return abg->seconds - bbg->seconds;
}

List_t *new_bgprocs() {
   List_t *bg_procs = calloc(1, sizeof(List_t));
   bg_procs->comparator = cmp_bgentry;
   return bg_procs;
}

static int sigchild_flag = 0;

void sigchild_handler(int sig) {
   if (sig == SIGCHLD) {
      sigchild_flag = 1;
   }
}

void add_bgprocs(List_t *bgprocs, job_info* job, pid_t pid) {
   bgentry_t* entry = malloc(sizeof(bgentry_t));
   entry->job = job;
   entry->pid = pid;
   entry->seconds = time(NULL);
   insertInOrder(bgprocs, entry);
}

bgentry_t* remove_bgprocs(List_t *bgprocs, pid_t pid) {
   node_t *head = bgprocs->head;
   int index = 0;
   while (head) {
      bgentry_t *entry = head->value;
      if (entry->pid == pid) {
         removeByIndex(bgprocs, index);
         return entry;
      }
      head = head->next;
      index += 1;
   }
   return NULL;
}

void reap_bgprocs(List_t *bgprocs) {
   if (sigchild_flag == 1) {
      pid_t pid;
      while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
         /* printf("waitpid = %d\n", pid); */
         bgentry_t *entry = remove_bgprocs(bgprocs, pid);
         if (entry) {
            printf(BG_TERM, entry->pid, entry->job->line);
            free_job(entry->job);
            free(entry);
         }
      }
      sigchild_flag = 0;
   }
}

void bglist(List_t *bgprocs) {
   node_t *head = bgprocs->head;
   while (head) {
      print_bgentry(head->value);
      head = head->next;
   }
}

void exit_bgprocs(List_t *bgprocs) {
   node_t *head = bgprocs->head;
   while (head) {
      bgentry_t *entry = head->value;
      kill(entry->pid, SIGTERM);
      head = head->next;
   }
   sigchild_flag = 1;
   while (bgprocs->head) {
      reap_bgprocs(bgprocs);
   }
   deleteList(bgprocs);
   free(bgprocs);
}

void siguser2_handler(int sig) {
   if (sig == SIGUSR2) {
      printf("Hi User! I am process %d\n", getpid());
   }
}

int run_procs(proc_info* proc, int infd, int outfd) {
   const int wmode = O_WRONLY | O_CREAT | O_TRUNC;
   if (proc->in_file && infd < 0) {
      infd = open(proc->in_file, O_RDONLY);
      if (infd < 0) {
         fprintf(stderr, RD_ERR);
         return -1;
      }
   }
   if (infd > 0) {
      dup2(infd, STDIN_FILENO);
      close(infd);
   }

   if (proc->out_file && outfd < 0) {
      outfd = open(proc->out_file, wmode, 0644);
      if (outfd < 0) {
         fprintf(stderr, RD_ERR);
         return -1;
      }
   }
   if (outfd > 0) {
      dup2(outfd, STDOUT_FILENO);
      close(outfd);
   }

   if (proc->err_file) {
      int errfd = open(proc->err_file, wmode, 0644);
      if (errfd < 0) {
         fprintf(stderr, RD_ERR);
         return -1;
      }
      dup2(errfd, STDERR_FILENO);
   }

   if (proc->outerr_file) {
      int errfd = open(proc->outerr_file, wmode, 0644);
      if (errfd < 0) {
         fprintf(stderr, RD_ERR);
         return -1;
      }
      dup2(errfd, STDERR_FILENO);
      dup2(errfd, STDOUT_FILENO);
      close(errfd);
   }

   int exec_result = execvp(proc->cmd, proc->argv);
   if (exec_result < 0) {  //Error checking
      printf(EXEC_ERR, proc->cmd);
      return -1;
   }
   return 0;
}

int check_redirect(proc_info* proc) {
   char *redirects[4];
   redirects[0] = proc->in_file;
   redirects[1] = proc->out_file;
   redirects[2] = proc->err_file;
   redirects[3] = proc->outerr_file;
   int i, j;
   for (i = 0; i < 4; ++i) {
      for (j = i+1; j < 4; ++j) {
         if (redirects[i] &&
             redirects[j] &&
             strcmp(redirects[i], redirects[j]) == 0) {
            return -1;
         }
      }
   }
   return 0;
}

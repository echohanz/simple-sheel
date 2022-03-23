#include "icssh.h"
#include "helpers.h"
#include "linkedList.h"
#include <readline/readline.h>
#include <string.h>

int main(int argc, char* argv[]) {
   int exec_result;
   int exit_status;
   pid_t pid;
   pid_t wait_result;
   char* line;
   List_t* bg_procs = new_bgprocs();
#ifdef GS
   rl_outstream = fopen("/dev/null", "w");
#endif

   // Setup segmentation fault handler
   if (signal(SIGSEGV, sigsegv_handler) == SIG_ERR) {
      perror("Failed to set signal handler");
      exit(EXIT_FAILURE);
   }
   if (signal(SIGCHLD, sigchild_handler) == SIG_ERR) {
      perror("Failed to set signal handler");
      exit(EXIT_FAILURE);
   }
   if (signal(SIGUSR2, siguser2_handler) == SIG_ERR) {
      perror("Failed to set signal handler");
      exit(EXIT_FAILURE);
   }

   // print the prompt & wait for the user to enter commands string
   while ((line = readline(SHELL_PROMPT)) != NULL) {
      reap_bgprocs(bg_procs);
      // MAGIC HAPPENS! Command string is parsed into a job struct
      // Will print out error message if command string is invalid
      job_info* job = validate_input(line);
      if (job == NULL) { // Command was empty string or invalid
         free(line);
         continue;
      }

      //Prints out the job linked list struture for debugging
#ifdef DEBUG   // If DEBUG flag removed in makefile, this will not longer print
      debug_print_job(job);
#endif

      // example built-in: exit
      if (strcmp(job->procs->cmd, "exit") == 0) {
         // Terminating the shell
         free(line);
         free_job(job);
         validate_input(NULL);   // calling validate_input with NULL will free the memory it has allocated
         exit_bgprocs(bg_procs);
         return 0;
      } else if (strcmp(job->procs->cmd, "cd") == 0) {
         cd(job->procs->argv[1]);
      } else if (strcmp(job->procs->cmd, "estatus") == 0) {
         estatus(exit_status);
      } else if (strcmp(job->procs->cmd, "bglist") == 0) {
         bglist(bg_procs);
      } else {
         int i;
         int fds[3][2];
         pid_t pids[3];
         int infd = -1, outfd = -1;
         proc_info *proc = job->procs;
         if (check_redirect(proc) < 0) {
            fprintf(stderr, RD_ERR);
            free(line);
            free_job(job);
            continue;
         }

         for (i = 0; i < job->nproc; ++i) {
            if (i + 1 < job->nproc) {
               if (pipe(fds[i]) < 0) {
                  fprintf(stderr, PIPE_ERR);
                  exit(EXIT_FAILURE);
               }
            }
            if (i > 0) {
               infd = fds[i-1][0];
            }
            if (i + 1 < job->nproc) {
               outfd = fds[i][1];
            }

            // example of good error handling!
            if ((pid = fork()) < 0) {
               perror("fork error");
               exit(EXIT_FAILURE);
            }

            if (pid == 0) {  //If zero, then it's the child process
               if (run_procs(proc, infd, outfd) < 0) {
                  // Cleaning up to make Valgrind happy 
                  // (not necessary because child will exit. Resources will be reaped by parent)
                  free_job(job);  
                  free(line);
                  validate_input(NULL);  // calling validate_input with NULL will free the memory it has allocated
                  exit(EXIT_FAILURE);
               }
            } else {
               if (infd > 0 ) {
                  close(infd);
               }
               if (outfd > 0) {
                  close(outfd);
               }
               pids[i] = pid;
            }
            proc = proc->next_proc;
         }

         if (!job->bg) {
            for (i = 0;i < job->nproc; ++i) {
                // As the parent, wait for the foreground job to finish
                wait_result = waitpid(pids[i], &exit_status, 0);
                if (wait_result < 0) {
                   printf(WAIT_ERR);
                   exit(EXIT_FAILURE);
                }
            }
         } else {
            add_bgprocs(bg_procs, job, pid);
         }
      }

      if (!job->bg) {
         free_job(job);  // if a foreground job, we no longer need the data
      }
      free(line);
   }

   // calling validate_input with NULL will free the memory it has allocated
   validate_input(NULL);

#ifndef GS
   fclose(rl_outstream);
#endif
   return 0;
}

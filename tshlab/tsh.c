
/*
 * This is a tsh.c program for a shell program. It includes SIGNINT, SIGCHLD
 * and SIGSTP handler. It will evaluate the parse-in command line tokens, and
 * executes and evaluates the valid command line for the user. It can do the
 * job foreground or background according to the command line, and also supports
 * built-in command such as ctrl-c, ctrl-z and jobs. It can also help the user
 * stopped or terminates a process peroperly according to the signal fired.
 */

#include "csapp.h"
#include "tsh_helper.h"

#include <sys/wait.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * If DEBUG is defined, enable contracts and printing on dbg_printf.
 */
#ifdef DEBUG
/* When debugging is enabled, these form aliases to useful functions */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_requires(...) assert(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#define dbg_ensures(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated for these */
#define dbg_printf(...)
#define dbg_requires(...)
#define dbg_assert(...)
#define dbg_ensures(...)
#endif

/* Function prototypes */
void eval(const char *cmdline);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);
void sigquit_handler(int sig);
void sigstp_sigint_handler(int sig);
void cleanup(void);

/* Self defined functions */
int builtin_command(struct cmdline_tokens token);
pid_t Fork(void);
void do_fg_bg_job(struct cmdline_tokens token);
bool valid_id(char *id);
int Open(struct cmdline_tokens token); // Open an output file safely
void Dup2(int file, int FILENO);       // safe version of dup2
// Safe version of execve
void Execve(const char *path, char *const *argv, char *const *envp);
// Safe version for add job
jid_t Add_job(pid_t pid, job_state state, const char *cmdline);
// delete_job safer version
bool Delete_job(jid_t jid);
// kill safer version
void Kill(pid_t, int sig);

/*
 * This function reads the argument from command line as input. It uses an while
 * loop to make the shell process continuing. It also determines the options
 * entered by the user. Flushes stdout if needed. Also, installed the sigchld,
 * sigstp and sigint handler. Then, called the eval function to evaluated the
 * command line input for job and process managing.
 */
int main(int argc, char **argv) {
  char c;
  char cmdline[MAXLINE_TSH]; // Cmdline for fgets
  bool emit_prompt = true;   // Emit prompt (default)

  // Redirect stderr to stdout (so that driver will get all output
  // on the pipe connected to stdout)
  if (dup2(STDOUT_FILENO, STDERR_FILENO) < 0) {
    perror("dup2 error");
    exit(1);
  }

  // Parse the command line
  while ((c = getopt(argc, argv, "hvp")) != EOF) {
    switch (c) {
    case 'h': // Prints help message
      usage();
      break;
    case 'v': // Emits additional diagnostic info
      verbose = true;
      break;
    case 'p': // Disables prompt printing
      emit_prompt = false;
      break;
    default:
      usage();
    }
  }

  // Create environment variable
  if (putenv("MY_ENV=42") < 0) {
    perror("putenv");
    exit(1);
  }

  // Set buffering mode of stdout to line buffering.
  // This prevents lines from being printed in the wrong order.
  if (setvbuf(stdout, NULL, _IOLBF, 0) < 0) {
    perror("setvbuf");
    exit(1);
  }

  // Initialize the job list
  init_job_list();

  // Register a function to clean up the job list on program termination.
  // The function may not run in the case of abnormal termination (e.g. when
  // using exit or terminating due to a signal handler), so in those cases,
  // we trust that the OS will clean up any remaining resources.
  if (atexit(cleanup) < 0) {
    perror("atexit");
    exit(1);
  }

  // Install the signal handlers
  Signal(SIGINT, sigint_handler);   // Handles Ctrl-C
  Signal(SIGTSTP, sigtstp_handler); // Handles Ctrl-Z
  Signal(SIGCHLD, sigchld_handler); // Handles terminated or stopped child

  Signal(SIGTTIN, SIG_IGN);
  Signal(SIGTTOU, SIG_IGN);

  Signal(SIGQUIT, sigquit_handler);

  // Execute the shell's read/eval loop
  while (true) {
    if (emit_prompt) {
      printf("%s", prompt);

      // We must flush stdout since we are not printing a full line.
      fflush(stdout);
    }

    if ((fgets(cmdline, MAXLINE_TSH, stdin) == NULL) && ferror(stdin)) {
      perror("fgets error");
      exit(1);
    }

    if (feof(stdin)) {
      // End of file (Ctrl-D)
      printf("\n");
      return 0;
    }

    // Remove any trailing newline
    char *newline = strchr(cmdline, '\n');
    if (newline != NULL) {
      *newline = '\0';
    }

    // Evaluate the command line
    eval(cmdline);
  }

  return -1; // control never reaches here
}

/*
 * This function evaluates the command line tokens. First, determines if a job
 * is a builtin command by calling the builtin_command function. Then,
 * it can handle the job to put it as foreground or background job to the job
 * list. Executes the child process in a process group with group id equals to
 * the pid of the parent process. It can also, blocks and unblocks signal
 * properly to eliminates race conditions. If the command line is empty or
 * there is a parse-in error, then jumps with an empty line. It can also shows
 * the current job list. File I/O redirection functionalities is also supported
 * by this function.
 */
void eval(const char *cmdline) {
  parseline_return parse_result;
  struct cmdline_tokens token;
  sigset_t prev_mask, mask_all;
  pid_t pid; /* Process id */

  // Parse command line
  parse_result = parseline(cmdline, &token);

  /* Ignores error parsing or empty lines */
  if (parse_result == PARSELINE_ERROR || parse_result == PARSELINE_EMPTY) {
    return;
  }

  // TODO: Implement commands here.
  sigfillset(&mask_all); // set the all signal mask

  if (!builtin_command(token)) { // if the line is not builtin command
    int file_in = 0;             // Initializes the input file
    int file_out = 0;            // Initializes the output file
    int olderrno = errno;

    // If the command has an input file
    if (token.infile) {
      // Terminates if the file cannot be open
      if ((file_in = open(token.infile, O_RDONLY)) < 0) {
        sio_eprintf("%s: %s\n", token.infile, strerror(errno));
        return;
      }
    }
    // If the command has a output file
    if (token.outfile) {
      // Terminates if the file cannot be open
      if ((file_out = Open(token)) < 0) {
        return;
      }
    }
    sigprocmask(SIG_BLOCK, &mask_all, &prev_mask); // Blocks signal
    if ((pid = Fork()) == 0) {                     /* Child runs the process */
      sigprocmask(SIG_SETMASK, &prev_mask, NULL);  // unblocks signal

      // Adds child to process group with group id: pid
      if (setpgid(0, 0) < 0) {
        sio_eprintf("setpgid error\n");
        exit(1);
      }

      // input file redirection
      if (token.infile && file_in >= 0) {
        Dup2(file_in, STDIN_FILENO);
        close(file_in); // close file
      }
      // output file redirection
      if (token.outfile && file_out >= 0) {
        Dup2(file_out, STDOUT_FILENO);
        close(file_out); // close file
      }

      // executes
      Execve(token.argv[0], token.argv, environ);
    }

    // Determines if the job state is foreground or background
    job_state js;
    if (parse_result == PARSELINE_FG) {
      js = FG;
    } else {
      js = BG;
    }

    /* add the job to the job list. If the job cannot be added, then exit
     */
    Add_job(pid, js, cmdline);

    /* Parent waits for foreground job to terminate */
    if (parse_result == PARSELINE_FG) { // Foreground job
      while (fg_job()) {
        sigsuspend(&prev_mask); // Waits for SIGCHLD signal
      }
    } else { // Background job
      sio_printf("[%d] (%d) %s\n", job_from_pid(pid), pid, cmdline);
    }

    sigprocmask(SIG_SETMASK, &prev_mask, NULL); // Unblocks signals

    errno = olderrno; // resets errno

  } else {
    if (token.builtin == BUILTIN_JOBS) { // For "jobs" command
      int olderrno = errno;
      int file_out;

      if (token.outfile) { // To redirect output file
        // Terminates if the output file cannot be open
        if ((file_out = Open(token)) < 0) {
          return;
        }
      } else {
        file_out = STDOUT_FILENO; // If file is valid, redirects
      }
      sigprocmask(SIG_BLOCK, &mask_all, &prev_mask); // Blocks signal
      if (list_jobs(file_out) == 0) {
        sigprocmask(SIG_SETMASK, &prev_mask, NULL); // Unblocks signal
        sio_eprintf("list job error\n");            // Prints error message
        exit(1);
      }
      if (token.outfile && file_out >= 0) {
        close(file_out); // Close the output file
      }
      errno = olderrno;                           // resets errno
      sigprocmask(SIG_SETMASK, &prev_mask, NULL); // Unblocks signal
    } else {
      do_fg_bg_job(token);
    }
  }
}

/*
 * This function is to open a output file with permissions. If a output file
 * cannot be open. Then prints the error message. Returns the output file
 * stream.
 */
int Open(struct cmdline_tokens token) {
  int file_out =
      open(token.outfile, O_WRONLY | O_CREAT | O_TRUNC,
           S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH | S_IRGRP | S_IWGRP);
  if (file_out < 0) {
    sio_eprintf("%s: %s\n", token.outfile, strerror(errno));
  }
  return file_out;
}

/*
 * Reads the command line token as input variable
 * Returns 0 for lines that are not builtin command, returns 1
 * otherwise.
 */
int builtin_command(struct cmdline_tokens token) {
  if (token.builtin == BUILTIN_NONE) {
    return 0;
  }
  if (token.builtin == BUILTIN_QUIT) {
    exit(0);
  } else {
    return 1;
  }
}

/*
 * This function will do foreground and background job all together. If the
 * command such as fg %jid or bg pid entered.
 */
void do_fg_bg_job(struct cmdline_tokens token) {
  pid_t pid;
  jid_t jid;
  sigset_t mask_all, prev_mask;
  sigfillset(&mask_all); // For blocking all signals

  /* extracts information from command lines */
  char *id;
  char *fg_or_bg;
  sigprocmask(SIG_BLOCK, &mask_all, &prev_mask); // Blocks signals
  // Determines if the job is foreground or background
  fg_or_bg = token.argv[0];
  id = token.argv[1];           // Determines the id the command
  if (!id || strlen(id) == 0) { // If the jid or pid is not missing
    sio_eprintf("%s command requires PID or %%jobid argument\n", fg_or_bg);
    sigprocmask(SIG_SETMASK, &prev_mask, NULL); // Unblocks signals
    return;
  }
  char jid_indicator = id[0]; // Indicates if this is a jib or not
  char *id_substring;
  if (jid_indicator == '%') {
    id_substring = id + 1; // Jumps above the '%' sign
  } else {
    id_substring = id;
  }
  // If this is a invalid id, then terminates
  if (!valid_id(id_substring)) {
    sio_eprintf("%s: argument must be a PID or %%jobid\n", fg_or_bg);
    sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    return;
  }
  if (jid_indicator == '%') {
    jid = (jid_t)atoi(id_substring); // Gets jid
  } else {
    pid = (pid_t)atoi(id_substring); // Gets pid
    // If the job is not existing, terminates
    if ((jid = job_from_pid(pid)) == 0) {
      sio_printf("%d: No such job\n", pid);
      sigprocmask(SIG_SETMASK, &prev_mask, NULL); // unblocks signal
      return;
    }
  }
  // If the job is not existing, terminates
  if (job_exists(jid) == 0) {
    sio_printf("%%%d: No such job\n", jid);
    sigprocmask(SIG_SETMASK, &prev_mask, NULL); // unblocks signal
    return;
  }

  pid = job_get_pid(jid); // gets back the pid
  Kill(-pid, SIGCONT);    // sends SIGCONT to all the processes in pid

  if (token.builtin == BUILTIN_FG) { // Foreground job
    job_set_state(jid, FG);          // Sets the job state back to foreground
    // Waits a foreground job to terminate
    while (fg_job()) {
      sigsuspend(&prev_mask);
    }
  } else {                  // Background job
    job_set_state(jid, BG); // Sets job state
    sio_printf("[%d] (%d) %s\n", job_from_pid(pid), pid, job_get_cmdline(jid));
  }
  sigprocmask(SIG_SETMASK, &prev_mask, NULL); // Unblocks signals
}

/*
 * Given a string id, determines if all the characters if this string is digit
 * if translated to number.
 */
bool valid_id(char *id) {
  int length = strlen(id);
  for (int i = 0; i < length; i++) {
    if (!isdigit(id[i])) {
      return false;
    }
  }
  return true;
}

/*****************
 * Signal handlers
 *****************/

/*
 * Sigichld handler for sigchld. This is aiming at the foreground job. If there
 * is an sigchld signal sent, and there is a foreground job ongoing, then
 * deletes this job with the indicated signal. Use waitpid method to wait
 * for a foreground job sent with sigchld signal. There are three cases, the
 * sigchld can be sent because of normally exited, stopped or terminated.
 */
void sigchld_handler(int sig) {
  int olderrno = errno;
  sigset_t mask_all, prev_all;
  pid_t pid;
  jid_t jid;
  int status;
  sigfillset(&mask_all);
  sigprocmask(SIG_BLOCK, &mask_all, &prev_all); // Blocks all signals
  while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
    jid = job_from_pid(pid); // Gets job id
    if (WIFEXITED(status)) { // Case when normally exited
      Delete_job(jid);
    } else if (WIFSTOPPED(status)) { // Case when status indicates stopped
      job_set_state(jid, ST);
      sio_printf("Job [%d] (%d) stopped by signal %d\n", jid, pid,
                 WSTOPSIG(status));
    } else if (WIFSIGNALED(status)) { // Case when status indicated terminated
      Delete_job(jid);
      sio_printf("Job [%d] (%d) terminated by signal %d\n", jid, pid,
                 WTERMSIG(status));
    }
  }
  sigprocmask(SIG_SETMASK, &prev_all, NULL); // Unblocks signal
  errno = olderrno;
}

/*
 * Sigint handler for sigint. This is aiming at the foreground job. If there
 * is an sigint signal sent, and there is a foreground job ongoing, then
 * terminates this job with the indicated signal
 */
void sigint_handler(int sig) { sigstp_sigint_handler(sig); }

/*
 * Sigstp handler for sigstp. This is aiming at the foreground job. If there
 * is an sigstp signal sent, and there is a foreground job ongoing, then
 * terminates this job with the indicated signal
 */
void sigtstp_handler(int sig) { sigstp_sigint_handler(sig); }

/*
 * Signal handler for sigint and sigstp. This is aiming at the foreground job.
 * If there is an sigint signal sent, and there is a foreground job ongoing,
 * then terminates this job with the indicated signal. This is the same frame
 * handler for sigint and sigstp
 */
void sigstp_sigint_handler(int sig) {
  int olderrno = errno;
  jid_t jid;
  sigset_t mask_all, prev_mask;
  sigfillset(&mask_all);                         // Masks all signals
  sigprocmask(SIG_BLOCK, &mask_all, &prev_mask); // blocks signals
  if ((jid = fg_job())) {
    Kill(-job_get_pid(jid), sig); // kill if there is a foreground job
  }
  sigprocmask(SIG_SETMASK, &prev_mask, NULL); // Unblocks signals
  errno = olderrno;                           // Resets errno
  return;
}

/*
 * cleanup - Attempt to clean up global resources when the program exits. In
 * particular, the job list must be freed at this time, since it may contain
 * leftover buffers from existing or even deleted jobs.
 */
void cleanup(void) {
  // Signals handlers need to be removed before destroying the joblist
  Signal(SIGINT, SIG_DFL);  // Handles Ctrl-C
  Signal(SIGTSTP, SIG_DFL); // Handles Ctrl-Z
  Signal(SIGCHLD, SIG_DFL); // Handles terminated or stopped child

  destroy_job_list();
}

/* Error handling fork function wrapper */
pid_t Fork(void) {
  pid_t pid;
  if ((pid = fork()) < 0) {
    sio_eprintf("fork error.\n");
  }
  return pid;
}

/*
 * Safe version dup2 function with error handling. If dup2 has an error, sends
 * an error message.
 */
void Dup2(int file, int FILENO) {
  if (dup2(file, FILENO) < 0) {
    sio_eprintf("dup2 error\n");
  }
}

/*
 * Safe version of execve. If the process cannot be executed, then prints an
 * error message.
 */
void Execve(const char *path, char *const *argv, char *const *envp) {
  if (execve(path, argv, envp) < 0) {
    sio_eprintf("failed to execute: %s\n", path);
  }
}

/*
 * Adds the job to the job list with a safer version. It will ensure that the
 * signals are blocked before the add_job function. Also, it will ensure that
 * the process id to be add to the job list must be a valid pid number.
 */
jid_t Add_job(pid_t pid, job_state state, const char *cmdline) {
  sigset_t mask_all, prev_mask;
  jid_t jid;
  sigfillset(&mask_all);
  if (pid <= 0) {
    _exit(1);
  }
  sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
  if ((jid = add_job(pid, state, cmdline)) == 0) {
    sigprocmask(SIG_SETMASK, &prev_mask, NULL); // unblocks signal
    sio_eprintf("Job: %d cannot be added to job list.\n", pid);
    exit(0);
  }
  return jid;
}

/*
 * Deletes the job from the job list with a precondition that it must be a
 * valid jid
 */
bool Delete_job(jid_t jid) {
  if (jid <= 0) {
    _exit(1);
  }
  return delete_job(jid);
}

/*
 * kill a process with given process id in a safer way. So that if the kill
 * fails, it can exit properly
 */
void Kill(pid_t pid, int sig) {
  if (kill(pid, sig) < 0) {
    _exit(1);
  }
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>
#include "ms.h"

// Command version
#define VERSION "0.1.0"

// Default interval in milliseconds
#define DEFAULT_INTERVAL 1000

// Max command args
#define ARGS_MAX 128

// Quiet mode
static int quiet = 0;

// Halt on failure
static int halt = 0;

// Output command usage
void usage() {
  printf(
      "\n"
      "  Usage: rss_watch [options] <cmd>\n"
      "\n"
      "  Options:\n"
      "\n"
      "    -q, --quiet         only output stderr\n"
      "    -x, --halt          halt on failure\n"
      "    -i, --interval <n>  interval in seconds or ms defaulting to 1\n"
      "    -t, --times <n>     number of times to execute\n"
      "    -v, --version       output version number\n"
      "    -h, --help          output this help information\n"
      "\n"
      );
  exit(1);
}

// Sleep in 'ms'
void mssleep(int ms) {
  struct timespec req = {0};
  time_t sec = (int)(ms / 1000);
  ms = ms - (sec * 1000);
  req.tv_sec = sec;
  req.tv_nsec = ms * 1000000L;
  while (-1 == nanosleep(&req, &req));
}

// Redirect stdout to 'path'
void redirect_stdout(const char *path) {
  int fd = open(path, O_WRONLY);
  if (dup2(fd, 1) < 0) {
    perror("dup2()");
    exit(1);
  }
}

// Check if 'arg' is the given short-opt or long-opt
int option(char *small, char *large, const char *arg) {
  if (!strcmp(small, arg) || !strcmp(large, arg))
    return 1;
  return 0;
}

// Return the total string-length consumed by 'strs'
int length(char **strs) {
  int n = 0;
  char *str;
  while ((str = *strs++))
    n += strlen(str);
  return n + 1;
}

// Join the given 'strs' with 'val'
char* join(char **strs, int len, char *val) {
  --len;
  char *buf = calloc(1, length(strs) + len * strlen(val) + 1);
  char *str;
  while ((str = *strs++)) {
    strcat(buf, str);
    if (*strs) strcat(buf, val);
  }
  return buf;
}

#define BUF_SZ 128
// get_rss
int get_rss(int pid) {
  char buf[BUF_SZ];
  char *token;
  FILE *fp;

  sprintf(buf, "/proc/%d/status", pid);
  fp = fopen(buf, "r");

  while (fgets(buf, BUF_SZ, fp)) {
    if (strncmp(buf, "VmRSS", 5))
      continue;

    token = strtok(&buf[6], "\t");
    break;
  }

  return atoi(token);
}

// get truncated millisecond times
long get_truncated_ms_time() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  
  return (tv.tv_sec % 1000000) * 1000 + tv.tv_usec / 1000;
}

// Parse argv
int main(int argc, const char **argv) {
  if (1 == argc)
    usage();
  int interval = DEFAULT_INTERVAL;
  int times = 1;

  int len = 0;
  int interpret= 1;
  char *args[ARGS_MAX] = {0};

  for (int i = 1; i < argc; i++) {
    const char *arg = argv[i];
    if (!interpret)
      goto arg;

    // -h, --help
    if (option("-h", "--help", arg))
      usage();

    // --q, --quiet
    if (option("-q", "--quiet", arg)) {
      quiet = 1;
      continue;
    }

    // -x, --halt
    if (option("-x", "--halt", arg)) {
      halt = 1;
      continue;
    }

    // -v, --version
    if (option("-v", "--version", arg)) {
      printf("%s\n", VERSION);
      exit(1);
    }

    // -i, --interval <n>
    if (option("-i", "--interval", arg)) {
      if (argc-1 == i) {
        fprintf(stderr, "\n  --interval requires an argument\n\n");
        exit(1);
      }

      arg = argv[++i];
      char last = arg[strlen(arg) - 1];
      //seconds or milliseconds
      interval = last >= 'a' && last <= 'z'
        ? string_to_milliseconds(arg)
        : atoi(arg) * 1000;
      continue;
    }

    // -t, --times <n>
    if (option("-t", "--times", arg)) {
      if (argc-1 == i) {
        fprintf(stderr, "\n  --times requires an argument\n\n");
        exit(1);
      }

      arg = argv[++i];
      times = atoi(arg);
      continue;
    }

    // cmd args
    if (len == ARGS_MAX) {
      fprintf(stderr, "number of arguments exceeded %d\n", len);
      exit(1);
    }

  arg:
    args[len++] = (char *) arg;
    interpret = 0;
  }

  // <cmd>
  if (!len) {
    fprintf(stderr, "\n  <cmd> required\n\n");
    exit(1);
  }

  // cmd
  //char *val = join(args, len, " ");
  //char *cmd[4] = {"sh", "-c", val, 0};
  //char *cmd[3] = {args[0], args, 0};

  //exec loop
  for (int t = 0; t < times; t++) {
    pid_t pid;
    int status;
    int max_rss = 0;

    switch (pid = fork()) {
      // error
      case -1:
        perror("fork()");
        exit(1);
      // child
      case 0:
        if (quiet)
          redirect_stdout("/dev/null");
        //execvp(cmd[0], cmd);
        execvp(args[0], args);
      // parent
      default:
        while (waitpid(pid, &status, WNOHANG) == 0) {
          int rss = get_rss(pid);
          printf("[RSS] %ld %d\n", get_truncated_ms_time(), rss);
          max_rss = (max_rss < rss) ? rss : max_rss;
          mssleep(interval);
        }

        // exit > 0
        if (WEXITSTATUS(status)) {
          fprintf(stderr, "\033[90mexit: %d\33[0m\n\n", WEXITSTATUS(status));
          if (halt)
            exit(WEXITSTATUS(status));
        }
    }
    printf("[RSS] %ld %d\n", get_truncated_ms_time(), 0);
    printf("\nPeak RSS: %d KB\n", max_rss);
  }

  return 0;
}


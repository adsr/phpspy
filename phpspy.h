#ifndef __PHPSPY_H
#define __PHPSPY_H

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <termbox.h>
#include <time.h>
#include <unistd.h>

#ifdef USE_ZEND
#include <main/php_config.h>
#undef ZEND_DEBUG
#define ZEND_DEBUG 0
#include <main/SAPI.h>
#undef snprintf
#undef vsnprintf
#undef HASH_ADD
#else
#include <php_structs_70.h>
#include <php_structs_71.h>
#include <php_structs_72.h>
#include <php_structs_73.h>
#endif

#ifdef USE_LIBDW
#include <elfutils/libdwfl.h>
#endif

#include "uthash.h"

#define PHPSPY_VERSION "0.3"
#define PHPSPY_MIN(a, b) ((a) < (b) ? (a) : (b))
#define PHPSPY_MAX(a, b) ((a) > (b) ? (a) : (b))
#define STR_LEN 256
#define try(__rv, __stmt) do { if (((__rv) = (__stmt) != 0)) return __rv; } while(0) // TODO use more or ditch

extern char *opt_pgrep_args;
extern int done;
extern int opt_num_workers;
extern pid_t opt_pid;

extern int main_pgrep();
extern int main_pid(pid_t pid);
extern void usage(FILE *fp, int exit_code);
#ifdef USE_TERMBOX
extern int main_top(int argc, char **argv);
#endif

#endif

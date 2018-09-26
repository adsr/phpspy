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
#include <time.h>
#include <unistd.h>

#ifdef USE_TERMBOX
#include <termbox.h>
#endif

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
#define PHPSPY_STR_LEN 256
#define try(__rv, __call) do { if (((__rv) = (__call)) != 0) return (__rv); } while(0)

#ifndef USE_ZEND
#define IS_UNDEF     0
#define IS_NULL      1
#define IS_FALSE     2
#define IS_TRUE      3
#define IS_LONG      4
#define IS_DOUBLE    5
#define IS_STRING    6
#define IS_ARRAY     7
#define IS_OBJECT    8
#define IS_RESOURCE  9
#define IS_REFERENCE 10
#endif

typedef struct varpeek_entry_s {
    #define PHPSPY_VARPEEK_KEY_SIZE 256
    #define PHPSPY_VARPEEK_VARNAME_SIZE 64
    char filename_lineno[PHPSPY_VARPEEK_KEY_SIZE];
    char varname[PHPSPY_VARPEEK_VARNAME_SIZE];
    UT_hash_handle hh;
} varpeek_entry_t;

typedef struct trace_context_s {
    pid_t pid;
    FILE *fout;
    uint64_t executor_globals_addr;
    uint64_t sapi_globals_addr;
    uint64_t core_globals_addr;
    char buf[PHPSPY_STR_LEN+1];
    int wrote_trace;
} trace_context_t;

extern char *opt_pgrep_args;
extern int done;
extern int opt_num_workers;
extern pid_t opt_pid;

extern int main_pgrep();
extern int main_pid(pid_t pid);
extern void usage(FILE *fp, int exit_code);
extern int get_symbol_addr(pid_t pid, const char *symbol, uint64_t *raddr);
#ifdef USE_TERMBOX
extern int main_top(int argc, char **argv);
#endif

#endif

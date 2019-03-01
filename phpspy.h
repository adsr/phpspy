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
#include <regex.h>

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
#include <php_structs_74.h>
#endif

#include <uthash.h>

#define try(__rv, __call) do { if (((__rv) = (__call)) != 0) return (__rv); } while(0)

#define PHPSPY_VERSION "0.4"
#define PHPSPY_MIN(a, b) ((a) < (b) ? (a) : (b))
#define PHPSPY_MAX(a, b) ((a) > (b) ? (a) : (b))
#define PHPSPY_STR_SIZE 256
#define PHPSPY_MAX_BUCKETS 256

#define PHPSPY_TRACE_EVENT_INIT        0
#define PHPSPY_TRACE_EVENT_STACK_BEGIN 1
#define PHPSPY_TRACE_EVENT_FRAME       2
#define PHPSPY_TRACE_EVENT_VARPEEK     3
#define PHPSPY_TRACE_EVENT_GLOPEEK     4
#define PHPSPY_TRACE_EVENT_REQUEST     5
#define PHPSPY_TRACE_EVENT_MEM         6
#define PHPSPY_TRACE_EVENT_STACK_END   7
#define PHPSPY_TRACE_EVENT_ERROR       8
#define PHPSPY_TRACE_EVENT_DEINIT      9

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

#if defined(__GNUC__) && __GNUC__ >= 4
# define PHPSPY_API __attribute__ ((visibility("default")))
#else
# define PHPSPY_API
#endif

typedef struct varpeek_var_s {
    char name[PHPSPY_STR_SIZE];
    UT_hash_handle hh;
} varpeek_var_t;

typedef struct varpeek_entry_s {
    char filename_lineno[PHPSPY_STR_SIZE];
    varpeek_var_t *varmap;
    UT_hash_handle hh;
} varpeek_entry_t;

typedef struct glopeek_entry_s {
    char key[PHPSPY_STR_SIZE];
    int index;
    char varname[PHPSPY_STR_SIZE];
    UT_hash_handle hh;
} glopeek_entry_t;

typedef struct trace_frame_s {
    char func[PHPSPY_STR_SIZE];
    char file[PHPSPY_STR_SIZE];
    char class[PHPSPY_STR_SIZE];
    size_t func_len;
    size_t file_len;
    size_t class_len;
    int lineno;
    int depth;
} trace_frame_t;

typedef struct trace_request_s {
    char uri[PHPSPY_STR_SIZE];
    char path[PHPSPY_STR_SIZE];
    char qstring[PHPSPY_STR_SIZE];
    char cookie[PHPSPY_STR_SIZE];
    double ts;
} trace_request_t;

typedef struct trace_mem_s {
    size_t size;
    size_t peak;
} trace_mem_t;

typedef struct trace_varpeek_s {
    varpeek_entry_t *entry;
    varpeek_var_t *var;
    char *zval_str;
} trace_varpeek_t;

typedef struct trace_glopeek_s {
    glopeek_entry_t *gentry;
    char *zval_str;
} trace_glopeek_t;

typedef struct trace_target_s {
    pid_t pid;
    uint64_t executor_globals_addr;
    uint64_t sapi_globals_addr;
    uint64_t core_globals_addr;
    uint64_t alloc_globals_addr;
} trace_target_t;

typedef struct trace_context_s {
    trace_target_t target;
    struct {
        trace_frame_t frame;
        trace_request_t request;
        trace_mem_t mem;
        trace_varpeek_t varpeek;
        trace_glopeek_t glopeek;
        char error[PHPSPY_STR_SIZE];
    } event;
    void *event_udata;
    int (*event_handler)(struct trace_context_s *context, int event_type);
    char buf[PHPSPY_STR_SIZE];
    size_t buf_len;
} trace_context_t;

typedef struct addr_memo_s {
    char php_bin_path[PHPSPY_STR_SIZE];
    uint64_t php_base_addr;
} addr_memo_t;

extern char *opt_pgrep_args;
extern int done;
extern int opt_num_workers;
extern pid_t opt_pid;
extern char *opt_frame_delim;
extern char *opt_trace_delim;
extern char *opt_path_output;
extern regex_t *opt_filter_re;
extern int opt_filter_negate;

extern int main_pgrep();
extern int main_pid(pid_t pid);
extern int main_top(int argc, char **argv);

extern void usage(FILE *fp, int exit_code);
extern int get_symbol_addr(addr_memo_t *memo, pid_t pid, const char *symbol, uint64_t *raddr);
extern int event_handler_fout(struct trace_context_s *context, int event_type);

PHPSPY_API int (*phpspy_event_handler)(trace_context_t *, int);

#endif

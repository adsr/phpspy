#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#ifdef USE_ZEND
#include <zend_API.h>
#else
#include <php_structs.h>
#endif

#define STR_LEN 256
#define MIN(a, b) ((a) < (b) ? (a) : (b))

pid_t opt_pid = -1;
long opt_sleep_ns = 10000000; // 10ms
unsigned long long opt_executor_globals_addr = 0;
unsigned long long opt_sapi_globals_addr = 0;
int opt_capture_req = 0;
int opt_max_stack_depth = -1;
char *opt_frame_delim = "\n";
char *opt_trace_delim = "\n\n";
unsigned long long opt_trace_limit = 0;

size_t zend_string_val_offset;
unsigned long long executor_globals_addr;
unsigned long long sapi_globals_addr;

static void usage(FILE *fp, int exit_code);
static void parse_opts(int argc, char **argv);
static void fork_child();
static int find_addresses();
static void dump_trace(pid_t pid, unsigned long long executor_globals_addr, unsigned long long sapi_globals_addr);
static int copy_proc_mem(pid_t pid, void *raddr, void *laddr, size_t size);
static void try_clock_gettime(struct timespec *ts);
static void calc_sleep_time(struct timespec *end, struct timespec *start, struct timespec *sleep);
static int get_symbol_addr(const char *symbol, unsigned long long *raddr);

static void usage(FILE *fp, int exit_code) {
    fprintf(fp, "Usage: phpspy [options] [--] <php_command>\n\n");
    fprintf(fp, "-h         Show help\n");
    fprintf(fp, "-p <pid>   Trace PHP process at pid\n");
    fprintf(fp, "-s <ns>    Sleep this many nanoseconds between traces (default: 10000000, 10ms)\n");
    fprintf(fp, "-n <max>   Set max stack trace depth to `max` (default: -1, unlimited)\n");
    fprintf(fp, "-x <hex>   Address of executor_globals in hex (default: 0, find dynamically)\n");
    fprintf(fp, "-a <hex>   Address of sapi_globals in hex (default: 0, find dynamically)\n");
    fprintf(fp, "-r         Capture request info as well\n");
    fprintf(fp, "-l <num>   Limit number of stack traces to capture (default: 0, unlimited)\n");
    exit(exit_code);
}

static void parse_opts(int argc, char **argv) {
    int c;
    while ((c = getopt(argc, argv, "hp:s:n:x:a:rl:")) != -1) {
        switch (c) {
            case 'h': usage(stdout, 0); break;
            case 'p': opt_pid = atoi(optarg); break;
            case 's': opt_sleep_ns = strtol(optarg, NULL, 10); break;
            case 'n': opt_max_stack_depth = atoi(optarg); break;
            case 'x': opt_executor_globals_addr = strtoull(optarg, NULL, 16); break;
            case 'a': opt_executor_globals_addr = strtoull(optarg, NULL, 16); break;
            case 'r': opt_capture_req = 1; break;
            case 'l': opt_trace_limit = strtoull(optarg, NULL, 10); break;
        }
    }
}

int main(int argc, char **argv) {
    unsigned long long n;
    struct timespec start_time, end_time, sleep_time;

    parse_opts(argc, argv);

    if (opt_pid == -1) {
        fork_child(argc, argv);
    }
    if (find_addresses()) {
        waitpid(opt_pid, NULL, 0);
        exit(1);
    }

    n = 0;
    while (1) {
        try_clock_gettime(&start_time);
        dump_trace(opt_pid, executor_globals_addr, sapi_globals_addr);
        try_clock_gettime(&end_time);
        calc_sleep_time(&end_time, &start_time, &sleep_time);
        nanosleep(&sleep_time, NULL);
        if (++n == opt_trace_limit) break;
    }

    return 0;
}

static void fork_child(int argc, char **argv) {
    if (optind >= argc) {
        fprintf(stderr, "Expected pid (-p) or command\n\n");
        usage(stderr, 1);
    }
    opt_pid = fork();
    if (opt_pid == 0) {
        execvp(argv[optind], argv + optind);
        perror("execvp");
        exit(1);
    } else if (opt_pid < 0) {
        perror("fork");
        exit(1);
    }
}

static int find_addresses() {
    if (get_symbol_addr("executor_globals", &executor_globals_addr) != 0) {
        return 1;
    }
    if (get_symbol_addr("sapi_globals", &sapi_globals_addr) != 0) {
        return 1;
    }
    zend_string_val_offset = offsetof(zend_string, val);
    return 0;
}

static void dump_trace(pid_t pid, unsigned long long executor_globals_addr, unsigned long long sapi_globals_addr) {
    char func[STR_LEN+1];
    char file[STR_LEN+1];
    char class[STR_LEN+1];
    char uri[STR_LEN+1];
    char path[STR_LEN+1];
    int file_len;
    int func_len;
    int class_len;
    int lineno;
    int depth;
    int wrote_trace;
    zend_execute_data* remote_execute_data;
    zend_executor_globals executor_globals;
    zend_execute_data execute_data;
    zend_op zop;
    zend_class_entry zce;
    zend_function zfunc;
    zend_string zstring;
    sapi_globals_struct sapi_globals;

    depth = 0;
    wrote_trace = 0;

    #define try_copy_proc_mem(__what, __raddr, __laddr, __size) do {        \
        if (copy_proc_mem(pid, (__raddr), (__laddr), (__size)) != 0) {      \
            fprintf(stderr, "dump_trace: Failed to copy %s\n", (__what));   \
            if (wrote_trace) printf("%s", opt_trace_delim);                 \
            return;                                                         \
        }                                                                   \
    } while(0)

    executor_globals.current_execute_data = NULL;
    try_copy_proc_mem("executor_globals", (void*)executor_globals_addr, &executor_globals, sizeof(executor_globals));
    remote_execute_data = executor_globals.current_execute_data;

    while (remote_execute_data && depth != opt_max_stack_depth) {
        memset(&execute_data, 0, sizeof(execute_data));
        memset(&zfunc, 0, sizeof(zfunc));
        memset(&zstring, 0, sizeof(zstring));
        memset(&zce, 0, sizeof(zce));
        memset(&zop, 0, sizeof(zop));
        memset(&sapi_globals, 0, sizeof(sapi_globals));

        try_copy_proc_mem("execute_data", remote_execute_data, &execute_data, sizeof(execute_data));
        try_copy_proc_mem("zfunc", execute_data.func, &zfunc, sizeof(zfunc));
        if (zfunc.common.function_name) {
            try_copy_proc_mem("function_name", zfunc.common.function_name, &zstring, sizeof(zstring));
            func_len = MIN(zstring.len, STR_LEN);
            try_copy_proc_mem("function_name.val", ((char*)zfunc.common.function_name) + zend_string_val_offset, func, func_len);
        } else {
            func_len = snprintf(func, sizeof(func), "<main>");
        }
        if (zfunc.common.scope) {
            try_copy_proc_mem("zce", zfunc.common.scope, &zce, sizeof(zce));
            try_copy_proc_mem("class_name", zce.name, &zstring, sizeof(zstring));
            class_len = MIN(zstring.len, STR_LEN);
            try_copy_proc_mem("class_name.val", ((char*)zce.name) + zend_string_val_offset, class, class_len);
            if (class_len+2 <= STR_LEN) {
                class[class_len+0] = ':';
                class[class_len+1] = ':';
                class[class_len+2] = '\0';
                class_len += 2;
            }
        } else {
            class[0] = '\0';
            class_len = 0;
        }
        if (zfunc.type == 2) {
            try_copy_proc_mem("zop", (void*)execute_data.opline, &zop, sizeof(zop));
            try_copy_proc_mem("filename", zfunc.op_array.filename, &zstring, sizeof(zstring));
            file_len = MIN(zstring.len, STR_LEN);
            try_copy_proc_mem("filename.val", ((char*)zfunc.op_array.filename) + zend_string_val_offset, file, file_len);
            lineno = zop.lineno;
        } else {
            file_len = snprintf(file, sizeof(file), "<internal>");
            lineno = -1;
        }
        printf("%d %.*s%.*s %.*s:%d%s", depth, class_len, class, func_len, func, file_len, file, lineno, opt_frame_delim);
        if (!wrote_trace) wrote_trace = 1;
        remote_execute_data = execute_data.prev_execute_data;
        depth += 1;
    }
    if (wrote_trace) {
        if (opt_capture_req) {
            try_copy_proc_mem("sapi_globals", (void*)sapi_globals_addr, &sapi_globals, sizeof(sapi_globals));
            if (sapi_globals.request_info.request_uri) {
                try_copy_proc_mem("uri", sapi_globals.request_info.request_uri, &uri, STR_LEN+1);
            } else {
                uri[0] = '-';
                uri[1] = '\0';
            }
            if (sapi_globals.request_info.path_translated) {
                try_copy_proc_mem("path", sapi_globals.request_info.path_translated, &path, STR_LEN+1);
            } else {
                path[0] = '-';
                path[1] = '\0';
            }
            printf("# %f %s %s%s", sapi_globals.global_request_time, uri, path, opt_trace_delim);
        } else {
            printf("# - - -%s", opt_trace_delim);
        }
    }
}

static int copy_proc_mem(pid_t pid, void *raddr, void *laddr, size_t size) {
    struct iovec local[1];
    struct iovec remote[1];
    local[0].iov_base = laddr;
    local[0].iov_len = size;
    remote[0].iov_base = raddr;
    remote[0].iov_len = size;
    if (process_vm_readv(pid, local, 1, remote, 1, 0) == -1) {
        if (errno == ESRCH) { // No such process
            perror("process_vm_readv");
            exit(1);
        }
        fprintf(stderr, "copy_proc_mem: %s; raddr=%p size=%lu\n", strerror(errno), raddr, size);
        return 1;
    }
    return 0;
}

#ifdef USE_LIBDW
#include "addr_libdw.c"
#else
#include "addr_readelf.c"
#endif

static void try_clock_gettime(struct timespec *ts) {
    if (clock_gettime(CLOCK_MONOTONIC_RAW, ts) == -1) {
        perror("clock_gettime");
        ts->tv_sec = 0;
        ts->tv_nsec = 0;
    }
}

static void calc_sleep_time(struct timespec *end, struct timespec *start, struct timespec *sleep) {
    long long end_ns, start_ns, sleep_ns;
    if (end->tv_sec == start->tv_sec) {
        sleep_ns = opt_sleep_ns - (end->tv_nsec - start->tv_nsec);
    } else {
        end_ns = (end->tv_sec * 1000000000ULL) + (end->tv_nsec * 1ULL);
        start_ns = (start->tv_sec * 1000000000ULL) + (start->tv_nsec * 1ULL);
        sleep_ns = opt_sleep_ns - (end_ns - start_ns);
    }
    if (sleep_ns < 0) {
        fprintf(stderr, "calc_sleep_time: Expected sleep_ns>0; decrease sample rate\n");
        sleep_ns = 0;
    }
    if (sleep_ns < 1000000000L) {
        sleep->tv_sec = 0;
        sleep->tv_nsec = (long)sleep_ns;
    } else {
        sleep->tv_sec = (long)sleep_ns / 1000000000L;
        sleep->tv_nsec = (long)sleep_ns - (sleep->tv_sec * 1000000000L);
    }
}

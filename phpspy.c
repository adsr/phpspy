#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <time.h>

#ifdef USE_ZEND
#include <main/SAPI.h>
#else
#include <php_structs_70.h>
#include <php_structs_71.h>
#include <php_structs_72.h>
#include <php_structs_73.h>
#endif

#define PHPSPY_VERSION "0.2"
#define STR_LEN 256
#define PHPSPY_MIN(a, b) ((a) < (b) ? (a) : (b))

pid_t opt_pid = -1;
char *opt_pgrep_args = NULL;
int opt_num_workers = 16;
long opt_sleep_ns = 10000000; // 10ms, 100Hz
unsigned long long opt_executor_globals_addr = 0;
unsigned long long opt_sapi_globals_addr = 0;
int opt_capture_req = 0;
int opt_capture_req_qstring = 1;
int opt_capture_req_cookie = 1;
int opt_capture_req_uri = 1;
int opt_capture_req_path = 1;
int opt_max_stack_depth = -1;
char *opt_frame_delim = "\n";
char *opt_trace_delim = "\n\n";
unsigned long long opt_trace_limit = 0;
char *opt_path_output = "-";
char *opt_path_child_out = "phpspy.%d.out";
char *opt_path_child_err = "phpspy.%d.err";
char *opt_phpv = "72";
int opt_pause = 0;

size_t zend_string_val_offset = 0;
unsigned long long executor_globals_addr = 0;
unsigned long long sapi_globals_addr = 0;
FILE *fout = NULL;
int done = 0;
void (*dump_trace_ptr)(pid_t, unsigned long long, unsigned long long) = NULL;

static void usage(FILE *fp, int exit_code);
static void parse_opts(int argc, char **argv);
extern int main_pid();
extern int main_pgrep();
static int main_fork(int argc, char **argv);
static void maybe_pause_pid(pid_t pid);
static void maybe_unpause_pid(pid_t pid);
static void redirect_child_stdio(int proc_fd, char *opt_path);
static void open_fout();
static int find_addresses();
static int copy_proc_mem(pid_t pid, void *raddr, void *laddr, size_t size);
static void try_clock_gettime(struct timespec *ts);
static void calc_sleep_time(struct timespec *end, struct timespec *start, struct timespec *sleep);
static int get_symbol_addr(const char *symbol, unsigned long long *raddr);
#ifdef USE_ZEND
static void dump_trace(pid_t pid, unsigned long long executor_globals_addr, unsigned long long sapi_globals_addr);
#else
static void dump_trace_70(pid_t pid, unsigned long long executor_globals_addr, unsigned long long sapi_globals_addr);
static void dump_trace_71(pid_t pid, unsigned long long executor_globals_addr, unsigned long long sapi_globals_addr);
static void dump_trace_72(pid_t pid, unsigned long long executor_globals_addr, unsigned long long sapi_globals_addr);
static void dump_trace_73(pid_t pid, unsigned long long executor_globals_addr, unsigned long long sapi_globals_addr);
#endif

static void usage(FILE *fp, int exit_code) {
    fprintf(fp, "Usage:\n");
    fprintf(fp, "  phpspy [options] -p <pid>\n");
    fprintf(fp, "  phpspy [options] -P <pgrep-args>\n");
    fprintf(fp, "  phpspy [options] -- <cmd>\n");
    fprintf(fp, "\n");
    fprintf(fp, "Options:\n");
    fprintf(fp, "  -h, --help                         Show this help\n");
    fprintf(fp, "  -p, --pid=<pid>                    Trace PHP process at `pid`\n");
    fprintf(fp, "  -P, --pgrep=<args>                 Concurrently trace processes that match\n");
    fprintf(fp, "                                       pgrep `args` (see also `-T`)\n");
    fprintf(fp, "  -T, --threads=<num>                Set number of threads to use with `-P`\n");
    fprintf(fp, "                                       (default: %d)\n", opt_num_workers);
    fprintf(fp, "  -s, --sleep-ns=<ns>                Sleep `ns` nanoseconds between traces\n");
    fprintf(fp, "                                       (see also `-H`) (default: %ld)\n", opt_sleep_ns);
    fprintf(fp, "  -H, --rate-hz=<hz>                 Trace `hz` times per second\n");
    fprintf(fp, "                                       (see also `-s`) (default: %llu)\n", 1000000000ULL/opt_sleep_ns);
    fprintf(fp, "  -V, --php-version=<ver>            Set PHP version\n");
    fprintf(fp, "                                       (default: %s; supported: 70 71 72 73)\n", opt_phpv);
    fprintf(fp, "  -l, --limit=<num>                  Limit total number of traces to capture\n");
    fprintf(fp, "                                       (default: %llu; 0=unlimited)\n", opt_trace_limit);
    fprintf(fp, "  -n, --max-depth=<max>              Set max stack trace depth\n");
    fprintf(fp, "                                       (default: %d; -1=unlimited)\n", opt_max_stack_depth);
    fprintf(fp, "  -r, --request-info                 Capture request info as well as traces\n");
    fprintf(fp, "  -R, --request-info-opts=<opts>     Set request info parts to capture (q=query\n");
    fprintf(fp, "                                       c=cookie u=uri p=path) (capital=negation)\n");
    fprintf(fp, "                                       (default: qcup, all)\n");
    fprintf(fp, "  -o, --output=<path>                Write phpspy output to `path`\n");
    fprintf(fp, "                                       (default: %s; -=stdout)\n", opt_path_output);
    fprintf(fp, "  -O, --child-stdout=<path>          Write child stdout to `path`\n");
    fprintf(fp, "                                       (default: %s)\n", opt_path_child_out);
    fprintf(fp, "  -E, --child-stderr=<path>          Write child stderr to `path`\n");
    fprintf(fp, "                                       (default: %s)\n", opt_path_child_err);
    fprintf(fp, "  -x, --addr-executor-globals=<hex>  Set address of executor_globals in hex\n");
    fprintf(fp, "                                       (default: %llu, 0=find dynamically)\n", opt_sapi_globals_addr);
    fprintf(fp, "  -a, --addr-sapi-globals=<hex>      Set address of sapi_globals in hex\n");
    fprintf(fp, "                                       (default: %llu; 0=find dynamically)\n", opt_executor_globals_addr);
    fprintf(fp, "  -S, --pause-process                Pause process while reading stacktrace\n");
    fprintf(fp, "                                       (unsafe for production!)\n");
    fprintf(fp, "  -1, --single-line                  Output in single-line mode\n");
    fprintf(fp, "  -#, --comment=<any>                Ignored; intended for self-documenting\n");
    fprintf(fp, "                                       commands\n");
    fprintf(fp, "  -v, --version                      Print phpspy version and exit\n");
    exit(exit_code);
}

static void parse_opts(int argc, char **argv) {
    int c;
    size_t i;
    struct option long_opts[] = {
        { "help",                  no_argument,       NULL, 'h' },
        { "pid",                   required_argument, NULL, 'p' },
        { "pgrep",                 required_argument, NULL, 'P' },
        { "threads",               required_argument, NULL, 'T' },
        { "sleep-ns",              required_argument, NULL, 's' },
        { "rate-hz",               required_argument, NULL, 'H' },
        { "php-version",           required_argument, NULL, 'V' },
        { "limit",                 required_argument, NULL, 'l' },
        { "max-depth",             required_argument, NULL, 'n' },
        { "request-info",          no_argument,       NULL, 'r' },
        { "request-info-opts",     required_argument, NULL, 'R' },
        { "output",                required_argument, NULL, 'o' },
        { "child-stdout",          required_argument, NULL, 'O' },
        { "child-stderr",          required_argument, NULL, 'E' },
        { "addr-executor-globals", required_argument, NULL, 'x' },
        { "addr-sapi-globals",     required_argument, NULL, 'a' },
        { "single-line",           no_argument,       NULL, '1' },
        { "comment",               optional_argument, NULL, '#' },
        { "version",               no_argument,       NULL, 'v' },
        { 0,                       0,                 0,    0   }
    };
    while ((c = getopt_long(argc, argv, "hp:P:T:s:H:V:l:n:rR:o:O:E:x:a:S1#:v", long_opts, NULL)) != -1) {
        switch (c) {
            case 'h': usage(stdout, 0); break;
            case 'p': opt_pid = atoi(optarg); break;
            case 'P': opt_pgrep_args = optarg; break;
            case 'T': opt_num_workers = atoi(optarg); break;
            case 's': opt_sleep_ns = strtol(optarg, NULL, 10); break;
            case 'H': opt_sleep_ns = (1000000000ULL / strtol(optarg, NULL, 10)); break;
            case 'V': opt_phpv = optarg; break;
            case 'l': opt_trace_limit = strtoull(optarg, NULL, 10); break;
            case 'n': opt_max_stack_depth = atoi(optarg); break;
            case 'r': opt_capture_req = 1; break;
            case 'R':
                for (i = 0; i < strlen(optarg); i++) {
                    switch (optarg[i]) {
                        case 'q': opt_capture_req_qstring = 1; break;
                        case 'c': opt_capture_req_cookie  = 1; break;
                        case 'u': opt_capture_req_uri     = 1; break;
                        case 'p': opt_capture_req_path    = 1; break;
                        case 'Q': opt_capture_req_qstring = 0; break;
                        case 'C': opt_capture_req_cookie  = 0; break;
                        case 'U': opt_capture_req_uri     = 0; break;
                        case 'P': opt_capture_req_path    = 0; break;
                    }
                }
                break;
            case 'o': opt_path_output = optarg; break;
            case 'O': opt_path_child_out = optarg; break;
            case 'E': opt_path_child_err = optarg; break;
            case 'x': opt_executor_globals_addr = strtoull(optarg, NULL, 16); break;
            case 'a': opt_sapi_globals_addr = strtoull(optarg, NULL, 16); break;
            case 'S': opt_pause = 1; break;
            case '1': opt_frame_delim = "\t"; opt_trace_delim = "\n"; break;
            case '#': break;
            case 'v':
                printf(
                    "phpspy v%s USE_ZEND=%s USE_LIBDW=%s\n",
                    PHPSPY_VERSION,
                    #ifdef USE_ZEND
                    "y",
                    #else
                    "n",
                    #endif
                    #ifdef USE_LIBDW
                    "y"
                    #else
                    "n"
                    #endif
                );
                exit(0);
        }
    }
}

int main(int argc, char **argv) {
    parse_opts(argc, argv);
    open_fout();

    if (opt_pid != -1) {
        return main_pid();
    } else if (opt_pgrep_args != NULL) {
        return main_pgrep();
    } else if (optind < argc) {
        return main_fork(argc, argv);
    }

    fprintf(stderr, "Expected pid (-p), pgrep (-P), or command\n\n");
    usage(stderr, 1);
    return 1;
}

int main_pid() {
    unsigned long long n;
    struct timespec start_time, end_time, sleep_time;

    if (find_addresses()) {
        waitpid(opt_pid, NULL, 0);
        exit(1);
    }

    #ifdef USE_ZEND
    dump_trace_ptr = dump_trace;
    #else
    if (strcmp("70", opt_phpv) == 0) {
        dump_trace_ptr = dump_trace_70;
    } else if (strcmp("71", opt_phpv) == 0) {
        dump_trace_ptr = dump_trace_71;
    } else if (strcmp("72", opt_phpv) == 0) {
        dump_trace_ptr = dump_trace_72;
    } else if (strcmp("73", opt_phpv) == 0) {
        dump_trace_ptr = dump_trace_73;
    } else {
        dump_trace_ptr = dump_trace_72;
    }
    #endif

    n = 0;
    while (!done) {
        try_clock_gettime(&start_time);
        maybe_pause_pid(opt_pid);
        dump_trace_ptr(opt_pid, executor_globals_addr, sapi_globals_addr);
        maybe_unpause_pid(opt_pid);
        try_clock_gettime(&end_time);
        calc_sleep_time(&end_time, &start_time, &sleep_time);
        nanosleep(&sleep_time, NULL);
        if (++n == opt_trace_limit) break;
    }

    return 0;
}

static int main_fork(int argc, char **argv) {
    (void)argc;
    opt_pid = fork();
    if (opt_pid == 0) {
        redirect_child_stdio(STDOUT_FILENO, opt_path_child_out);
        redirect_child_stdio(STDERR_FILENO, opt_path_child_err);
        execvp(argv[optind], argv + optind);
        perror("execvp");
        exit(1);
    } else if (opt_pid < 0) {
        perror("fork");
        exit(1);
    }
    return main_pid();
}

static void maybe_pause_pid(pid_t pid) {
    if (!opt_pause) return;
    if (ptrace(PTRACE_ATTACH, pid, 0, 0) == -1) {
        perror("ptrace");
        exit(1);
    }
    if (waitpid(pid, NULL, 0) == -1) {
        perror("waitpid");
        exit(1);
    }
}

static void maybe_unpause_pid(pid_t pid) {
    if (!opt_pause) return;
    if (ptrace(PTRACE_DETACH, pid, 0, 0) == -1) {
        perror("ptrace");
        exit(1);
    }
}

static void redirect_child_stdio(int proc_fd, char *opt_path) {
    char *redir_path;
    FILE *redir_file;
    if (strcmp(opt_path, "-") == 0) {
        return;
    } else if (strstr(opt_path, "%d") != NULL) {
        if (asprintf(&redir_path, opt_path, getpid()) < 0) {
            errno = ENOMEM;
            perror("asprintf");
            exit(1);
        }
    } else {
        if ((redir_path = strdup(opt_path)) == NULL) {
            perror("strdup");
            exit(1);
        }
    }
    if ((redir_file = fopen(redir_path, "w")) == NULL) {
        perror("fopen");
        free(redir_path);
        exit(1);
    }
    dup2(fileno(redir_file), proc_fd);
    fclose(redir_file);
    free(redir_path);
}

static void open_fout() {
    if (strcmp(opt_path_output, "-") == 0) {
        fout = fdopen(STDOUT_FILENO, "w");
    } else {
        fout = fopen(opt_path_output, "w");
    }
    setvbuf(fout, NULL, _IOLBF, PIPE_BUF);
    if (!fout) {
        perror("fopen");
        exit(1);
    }
    // TODO fclose(fout)
}

static int find_addresses() {
    if (opt_executor_globals_addr != 0) {
        executor_globals_addr = opt_executor_globals_addr;
    } else if (get_symbol_addr("executor_globals", &executor_globals_addr) != 0) {
        return 1;
    }
    if (opt_sapi_globals_addr != 0) {
        sapi_globals_addr = opt_sapi_globals_addr;
    } else if (get_symbol_addr("sapi_globals", &sapi_globals_addr) != 0) {
        return 1;
    }
    #ifdef USE_ZEND
    zend_string_val_offset = offsetof(zend_string, val);
    #else
    zend_string_val_offset = offsetof(zend_string_70, val);
    #endif
    return 0;
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

#ifdef USE_ZEND
#include "phpspy_trace.c"
#else
#define phpv 70
#include "phpspy_trace_tpl.c"
#undef phpv
#define phpv 71
#include "phpspy_trace_tpl.c"
#undef phpv
#define phpv 72
#include "phpspy_trace_tpl.c"
#undef phpv
#define phpv 73
#include "phpspy_trace_tpl.c"
#undef phpv
#endif

#ifdef USE_LIBDW
#include "addr_libdw.c"
#else
#include "addr_readelf.c"
#endif

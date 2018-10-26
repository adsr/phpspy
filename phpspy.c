#include "phpspy.h"

pid_t opt_pid = -1;
char *opt_pgrep_args = NULL;
int opt_num_workers = 16;
int opt_top_mode = 0;
long opt_sleep_ns = 10101010; /* ~99Hz */
uint64_t opt_executor_globals_addr = 0;
uint64_t opt_sapi_globals_addr = 0;
int opt_capture_req = 0;
int opt_capture_req_qstring = 0;
int opt_capture_req_cookie = 0;
int opt_capture_req_uri = 0;
int opt_capture_req_path = 0;
int opt_max_stack_depth = -1;
char *opt_frame_delim = "\n";
char *opt_trace_delim = "\n";
uint64_t opt_trace_limit = 0;
char *opt_path_output = "-";
char *opt_path_child_out = "phpspy.%d.out";
char *opt_path_child_err = "phpspy.%d.err";
char *opt_phpv = NULL;
int opt_pause = 0;

size_t zend_string_val_offset = 0;
int done = 0;
int (*do_trace_ptr)(trace_context_t *context) = NULL;
varpeek_entry_t *varpeek_map = NULL;

static void parse_opts(int argc, char **argv);
static int main_fork(int argc, char **argv);
static int pause_pid(pid_t pid);
static int unpause_pid(pid_t pid);
static void redirect_child_stdio(int proc_fd, char *opt_path);
static int find_addresses(trace_target_t *target);
static void get_clock_time(struct timespec *ts);
static void calc_sleep_time(struct timespec *end, struct timespec *start, struct timespec *sleep);
static int copy_proc_mem(trace_context_t *context, const char *what, void *raddr, void *laddr, size_t size);
static void varpeek_add(char *varspec);
static void try_get_php_version(char **phpv);

#ifdef USE_ZEND
static int do_trace(trace_context_t *context);
#else
static int do_trace_70(trace_context_t *context);
static int do_trace_71(trace_context_t *context);
static int do_trace_72(trace_context_t *context);
static int do_trace_73(trace_context_t *context);
static int do_trace_74(trace_context_t *context);
#endif

void usage(FILE *fp, int exit_code) {
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
    fprintf(fp, "                                       (see also `-s`) (default: %lu)\n", 1000000000UL/opt_sleep_ns);
    fprintf(fp, "  -V, --php-version=<ver>            Set PHP version\n");
    fprintf(fp, "                                       (default: %s; supported: 70 71 72 73 74)\n", opt_phpv);
    fprintf(fp, "  -l, --limit=<num>                  Limit total number of traces to capture\n");
    fprintf(fp, "                                       (default: %lu; 0=unlimited)\n", opt_trace_limit);
    fprintf(fp, "  -n, --max-depth=<max>              Set max stack trace depth\n");
    fprintf(fp, "                                       (default: %d; -1=unlimited)\n", opt_max_stack_depth);
    fprintf(fp, "  -r, --request-info=<opts>          Set request info parts to capture (q=query\n");
    fprintf(fp, "                                       c=cookie u=uri p=path) (capital=negation)\n");
    fprintf(fp, "                                       (default: QCUP; none)\n");
    fprintf(fp, "  -o, --output=<path>                Write phpspy output to `path`\n");
    fprintf(fp, "                                       (default: %s; -=stdout)\n", opt_path_output);
    fprintf(fp, "  -O, --child-stdout=<path>          Write child stdout to `path`\n");
    fprintf(fp, "                                       (default: %s)\n", opt_path_child_out);
    fprintf(fp, "  -E, --child-stderr=<path>          Write child stderr to `path`\n");
    fprintf(fp, "                                       (default: %s)\n", opt_path_child_err);
    fprintf(fp, "  -x, --addr-executor-globals=<hex>  Set address of executor_globals in hex\n");
    fprintf(fp, "                                       (default: %lu; 0=find dynamically)\n", opt_sapi_globals_addr);
    fprintf(fp, "  -a, --addr-sapi-globals=<hex>      Set address of sapi_globals in hex\n");
    fprintf(fp, "                                       (default: %lu; 0=find dynamically)\n", opt_executor_globals_addr);
    fprintf(fp, "  -1, --single-line                  Output in single-line mode\n");
    fprintf(fp, "  -#, --comment=<any>                Ignored; intended for self-documenting\n");
    fprintf(fp, "                                       commands\n");
    fprintf(fp, "  -@, --nothing                      Ignored\n");
    fprintf(fp, "  -v, --version                      Print phpspy version and exit\n");
    fprintf(fp, "\n");
    fprintf(fp, "Experimental Features:\n");
    fprintf(fp, "  -S, --pause-process                Pause process while reading stacktrace\n");
    fprintf(fp, "                                       (unsafe for production!)\n");
    fprintf(fp, "  -e, --peek-var=<varspec>           Peek at the contents of the variable located\n");
    fprintf(fp, "                                       at `varspec`, which has the format:\n");
    fprintf(fp, "                                       <varname>@<path>:<lineno>\n");
    fprintf(fp, "                                       e.g., xyz@/path/to.php:1234\n");
    fprintf(fp, "  -t, --top                          Show dynamic top-like output\n");
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
        { "top",                   no_argument,       NULL, 't' },
        { "peek-var",              required_argument, NULL, 'e' },
        { "sleep-ns",              required_argument, NULL, 's' },
        { "rate-hz",               required_argument, NULL, 'H' },
        { "php-version",           required_argument, NULL, 'V' },
        { "limit",                 required_argument, NULL, 'l' },
        { "max-depth",             required_argument, NULL, 'n' },
        { "request-info",          required_argument, NULL, 'r' },
        { "output",                required_argument, NULL, 'o' },
        { "child-stdout",          required_argument, NULL, 'O' },
        { "child-stderr",          required_argument, NULL, 'E' },
        { "addr-executor-globals", required_argument, NULL, 'x' },
        { "addr-sapi-globals",     required_argument, NULL, 'a' },
        { "single-line",           no_argument,       NULL, '1' },
        { "comment",               required_argument, NULL, '#' },
        { "nothing",               no_argument,       NULL, '@' },
        { "version",               no_argument,       NULL, 'v' },
        { 0,                       0,                 0,    0   }
    };
    while ((c = getopt_long(argc, argv, "hp:P:T:te:s:H:V:l:n:r:o:O:E:x:a:S1#:@v", long_opts, NULL)) != -1) {
        switch (c) {
            case 'h': usage(stdout, 0); break;
            case 'p': opt_pid = atoi(optarg); break;
            case 'P': opt_pgrep_args = optarg; break;
            case 'T': opt_num_workers = atoi(optarg); break;
            case 't': opt_top_mode = 1; break;
            case 'e': varpeek_add(optarg); break;
            case 's': opt_sleep_ns = strtol(optarg, NULL, 10); break;
            case 'H': opt_sleep_ns = (1000000000UL / strtol(optarg, NULL, 10)); break;
            case 'V': opt_phpv = optarg; break;
            case 'l': opt_trace_limit = strtoull(optarg, NULL, 10); break;
            case 'n': opt_max_stack_depth = atoi(optarg); break;
            case 'r':
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
                opt_capture_req = opt_capture_req_qstring | opt_capture_req_cookie | opt_capture_req_uri | opt_capture_req_path;
                break;
            case 'o': opt_path_output = optarg; break;
            case 'O': opt_path_child_out = optarg; break;
            case 'E': opt_path_child_err = optarg; break;
            case 'x': opt_executor_globals_addr = strtoull(optarg, NULL, 16); break;
            case 'a': opt_sapi_globals_addr = strtoull(optarg, NULL, 16); break;
            case 'S': opt_pause = 1; break;
            case '1': opt_frame_delim = "\t"; opt_trace_delim = "\n"; break;
            case '#': break;
            case '@': break;
            case 'v':
                printf(
                    "phpspy v%s USE_TERMBOX=%s USE_ZEND=%s\n",
                    PHPSPY_VERSION,
                    #ifdef USE_TERMBOX
                    "y",
                    #else
                    "n",
                    #endif
                    #ifdef USE_ZEND
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

    if (opt_top_mode != 0) {
        #ifdef USE_TERMBOX
        return main_top(argc, argv);
        #else
        fprintf(stderr, "Please recompile phpspy with USE_TERMBOX=1 for top mode support.\n");
        return 1;
        #endif
    } else if (opt_pid != -1) {
        return main_pid(opt_pid);
    } else if (opt_pgrep_args != NULL) {
        return main_pgrep();
    } else if (optind < argc) {
        return main_fork(argc, argv);
    }

    fprintf(stderr, "Expected pid (-p), pgrep (-P), or command\n\n");
    usage(stderr, 1);
    return 1;
}

int main_pid(pid_t pid) {
    int rv;
    uint64_t n;
    trace_context_t context;
    struct timespec start_time, end_time, sleep_time;

    memset(&context, 0, sizeof(trace_context_t));
    context.target.pid = pid;
    context.event_handler = event_handler_fout; /* TODO set based on option */
    try(rv, find_addresses(&context.target));
    try(rv, context.event_handler(&context, PHPSPY_TRACE_EVENT_INIT));

    #ifdef USE_ZEND
    do_trace_ptr = do_trace;
    #else


    if (opt_phpv == NULL) {
      try_get_php_version(&opt_phpv);
    }

    if (strcmp("70", opt_phpv) == 0) {
        do_trace_ptr = do_trace_70;
    } else if (strcmp("71", opt_phpv) == 0) {
        do_trace_ptr = do_trace_71;
    } else if (strcmp("72", opt_phpv) == 0) {
        do_trace_ptr = do_trace_72;
    } else if (strcmp("73", opt_phpv) == 0) {
        do_trace_ptr = do_trace_73;
    } else if (strcmp("74", opt_phpv) == 0) {
        do_trace_ptr = do_trace_74;
    } else {
        do_trace_ptr = do_trace_72;
    }
    #endif

    n = 0;
    while (!done) {
        rv = 0;
        get_clock_time(&start_time);
        if (opt_pause) rv |= pause_pid(pid);
        rv |= do_trace_ptr(&context);
        if (opt_pause) rv |= unpause_pid(pid);
        if ((rv == 0 && ++n == opt_trace_limit) || (rv & 2) != 0) break;
        get_clock_time(&end_time);
        calc_sleep_time(&end_time, &start_time, &sleep_time);
        nanosleep(&sleep_time, NULL);
    }

    /* TODO proper signal handling for non-pgrep modes */
    context.event_handler(&context, PHPSPY_TRACE_EVENT_DEINIT);
    return 0;
}

static int main_fork(int argc, char **argv) {
    int rv, status;
    pid_t fork_pid;
    (void)argc;
    fork_pid = fork();
    if (fork_pid == 0) {
        redirect_child_stdio(STDOUT_FILENO, opt_path_child_out);
        redirect_child_stdio(STDERR_FILENO, opt_path_child_err);
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execvp(argv[optind], argv + optind);
        perror("execvp");
        exit(1);
    } else if (fork_pid < 0) {
        perror("fork");
        exit(1);
    }
    waitpid(fork_pid, &status, 0);
    if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGTRAP) {
        fprintf(stderr, "main_fork: Expected SIGTRAP from child\n");
    }
    ptrace(PTRACE_DETACH, fork_pid, NULL, NULL);
    rv = main_pid(fork_pid);
    waitpid(fork_pid, NULL, 0);
    return rv;
}

static int pause_pid(pid_t pid) {
    int rv;
    if (ptrace(PTRACE_ATTACH, pid, 0, 0) == -1) {
        rv = errno;
        perror("ptrace");
        return rv == ESRCH ? 2 : 1;
    }
    if (waitpid(pid, NULL, 0) < 0) {
        perror("waitpid");
        return 1;
    }
    return 0;
}

static int unpause_pid(pid_t pid) {
    int rv;
    if (ptrace(PTRACE_DETACH, pid, 0, 0) == -1) {
        rv = errno;
        perror("ptrace");
        return rv == ESRCH ? 2 : 1;
    }
    return 0;
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

static int find_addresses(trace_target_t *target) {
    if (opt_executor_globals_addr != 0) {
        target->executor_globals_addr = opt_executor_globals_addr;
    } else if (get_symbol_addr(target->pid, "executor_globals", &target->executor_globals_addr) != 0) {
        return 1;
    }
    if (opt_sapi_globals_addr != 0) {
        target->sapi_globals_addr = opt_sapi_globals_addr;
    } else if (get_symbol_addr(target->pid, "sapi_globals", &target->sapi_globals_addr) != 0) {
        return 1;
    }
    if (get_symbol_addr(target->pid, "core_globals", &target->core_globals_addr) != 0) {
        /* TODO opt_core_globals_addr */
        return 1;
    }
    /* TODO probably don't need zend_string_val_offset */
    #ifdef USE_ZEND
    zend_string_val_offset = offsetof(zend_string, val);
    #else
    zend_string_val_offset = offsetof(zend_string_70, val);
    #endif
    return 0;
}

static void get_clock_time(struct timespec *ts) {
    if (clock_gettime(CLOCK_MONOTONIC_RAW, ts) == -1) {
        perror("clock_gettime");
        ts->tv_sec = 0;
        ts->tv_nsec = 0;
    }
}

static void calc_sleep_time(struct timespec *end, struct timespec *start, struct timespec *sleep) {
    long end_ns, start_ns, sleep_ns;
    if (end->tv_sec == start->tv_sec) {
        sleep_ns = opt_sleep_ns - (end->tv_nsec - start->tv_nsec);
    } else {
        end_ns = (end->tv_sec * 1000000000UL) + (end->tv_nsec * 1UL);
        start_ns = (start->tv_sec * 1000000000UL) + (start->tv_nsec * 1UL);
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

static void varpeek_add(char *varspec) {
    char *at_sign;
    varpeek_entry_t *varpeek;
    at_sign = strstr(varspec, "@");
    if (!at_sign) {
        fprintf(stderr, "varpeek_add: Malformed varspec: %s\n\n", varspec);
        usage(stderr, 1);
    }
    varpeek = malloc(sizeof(varpeek_entry_t)); /* TODO free */
    snprintf(varpeek->filename_lineno, PHPSPY_VARPEEK_KEY_SIZE, "%s", at_sign+1);
    snprintf(varpeek->varname, PHPSPY_VARPEEK_VARNAME_SIZE, "%.*s", (int)(at_sign-varspec), varspec);
    HASH_ADD_STR(varpeek_map, filename_lineno, varpeek);
}

static int copy_proc_mem(trace_context_t *context, const char *what, void *raddr, void *laddr, size_t size) {
    int rv;
    struct iovec local[1];
    struct iovec remote[1];
    local[0].iov_base = laddr;
    local[0].iov_len = size;
    remote[0].iov_base = raddr;
    remote[0].iov_len = size;
    rv = 0;
    if (process_vm_readv(context->target.pid, local, 1, remote, 1, 0) == -1) {
        if (errno == ESRCH) { /* No such process */
            perror("process_vm_readv");
            rv = 2; /* Return value of 2 tells main_pid to exit */
        } else {
            fprintf(stderr, "copy_proc_mem: Failed to copy %s; err=%s raddr=%p size=%lu\n", what, strerror(errno), raddr, size);
            rv = 1;
        }
    }
    return rv;
}

void try_get_php_version(char **phpv) {
    FILE *pcmd;
    char *which_cmd;
    char *strings_cmd;
    char *php_path = NULL;
    size_t php_path_size = 4096;
    ssize_t size;
    char *phpv_raw = NULL;
    size_t phpv_raw_size = 64;

    if (asprintf(&which_cmd, "which php") < 0) {
        errno = ENOMEM;
        perror("asprintf");
        return;
    }

    if ((pcmd = popen(which_cmd, "r")) != NULL) {
        size = getline(&php_path, &php_path_size, pcmd);
        if (size <= 0) {
            perror("Unable to locate path to php binary");
            return;
        }
        php_path[size - 1] = 0;
        pclose(pcmd);
    }
    free(which_cmd);

    if (asprintf(&strings_cmd, "strings -d %s | grep \"X-Powered-By\"", php_path) < 0) {
        errno = ENOMEM;
        perror("asprintf");
        return;
    }

    if ((pcmd = popen(strings_cmd, "r")) != NULL) {
        size = getline(&phpv_raw, &phpv_raw_size, pcmd);
        if (size <= 0) {
            perror("Unable to determine php version");
            return;
        }

        if (asprintf(phpv, "%c%c", phpv_raw[18], phpv_raw[20]) < 0) {
            errno = ENOMEM;
            perror("asprintf");
            return;
        }
        pclose(pcmd);
    }
    free(strings_cmd);
    free(php_path);
    free(phpv_raw);
}

/* TODO figure out a way to make this cleaner */
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
#define phpv 74
#include "phpspy_trace_tpl.c"
#undef phpv
#endif

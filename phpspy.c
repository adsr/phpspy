#include "phpspy.h"

#define TB_IMPL
#define TB_OPT_V1_COMPAT
#include "termbox.h"
#undef TB_IMPL

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
int opt_capture_mem = 0;
int opt_max_stack_depth = -1;
char opt_frame_delim = '\n';
char opt_trace_delim = '\n';
uint64_t opt_trace_limit = 0;
long opt_time_limit_ms = 0;
char *opt_path_output = "-";
char *opt_path_child_out = "phpspy.%d.out";
char *opt_path_child_err = "phpspy.%d.err";
char *opt_phpv = "auto";
int opt_pause = 0;
regex_t *opt_filter_re = NULL;
int opt_filter_negate = 0;
int opt_verbose_fields_pid = 0;
int opt_verbose_fields_ts = 0;
int (*opt_event_handler)(struct trace_context_s *context, int event_type) = event_handler_fout;
char *opt_event_handler_opts = NULL;
int opt_continue_on_error = 0;
int opt_fout_buffer_size = 4096;

int done = 0;
int (*do_trace_ptr)(trace_context_t *context) = NULL;
varpeek_entry_t *varpeek_map = NULL;
glopeek_entry_t *glopeek_map = NULL;
regex_t filter_re;
int log_error_enabled = 1;
int in_pgrep_mode = 0;
uint64_t trace_count = 0;

static void parse_opts(int argc, char **argv);
static int main_fork(int argc, char **argv);
static void cleanup();
static int pause_pid(pid_t pid);
static int unpause_pid(pid_t pid);
static void redirect_child_stdio(int proc_fd, char *opt_path);
static int find_addresses(trace_target_t *target);
static void clock_get(struct timespec *ts);
static void clock_add(struct timespec *a, struct timespec *b, struct timespec *res);
static int clock_diff(struct timespec *a, struct timespec *b);
static void calc_sleep_time(struct timespec *end, struct timespec *start, struct timespec *sleep);
static void varpeek_add(char *varspec);
static void glopeek_add(char *glospec);
static int copy_proc_mem(pid_t pid, const char *what, void *raddr, void *laddr, size_t size);

#ifdef USE_ZEND
static int do_trace(trace_context_t *context);
#else
static int get_php_version(trace_target_t *target);
static int do_trace_70(trace_context_t *context);
static int do_trace_71(trace_context_t *context);
static int do_trace_72(trace_context_t *context);
static int do_trace_73(trace_context_t *context);
static int do_trace_74(trace_context_t *context);
static int do_trace_80(trace_context_t *context);
static int do_trace_81(trace_context_t *context);
static int do_trace_82(trace_context_t *context);
static int do_trace_83(trace_context_t *context);
#endif

int main(int argc, char **argv) {
    int rv;
    parse_opts(argc, argv);

    if (opt_top_mode != 0) {
        rv = main_top(argc, argv);
    } else if (opt_pid != -1) {
        rv = main_pid(opt_pid);
    } else if (opt_pgrep_args != NULL) {
        in_pgrep_mode = 1;
        rv = main_pgrep();
    } else if (optind < argc) {
        rv = main_fork(argc, argv);
    } else {
        log_error("Expected pid (-p), pgrep (-P), or command\n\n");
        usage(stderr, 1);
        rv = 1;
    }
    cleanup();
    return rv;
}

void usage(FILE *fp, int exit_code) {
    fprintf(fp, "Usage:\n");
    fprintf(fp, "  phpspy [options] -p <pid>\n");
    fprintf(fp, "  phpspy [options] -P <pgrep-args>\n");
    fprintf(fp, "  phpspy [options] [--] <cmd>\n");
    fprintf(fp, "\n");
    fprintf(fp, "Options:\n");
    fprintf(fp, "  -h, --help                         Show this help\n");
    fprintf(fp, "  -p, --pid=<pid>                    Trace PHP process at `pid`\n");
    fprintf(fp, "  -P, --pgrep=<args>                 Concurrently trace processes that\n");
    fprintf(fp, "                                       match pgrep `args` (see also `-T`)\n");
    fprintf(fp, "  -T, --threads=<num>                Set number of threads to use with `-P`\n");
    fprintf(fp, "                                       (default: %d)\n", opt_num_workers);
    fprintf(fp, "  -s, --sleep-ns=<ns>                Sleep `ns` nanoseconds between traces\n");
    fprintf(fp, "                                       (see also `-H`) (default: %ld)\n", opt_sleep_ns);
    fprintf(fp, "  -H, --rate-hz=<hz>                 Trace `hz` times per second\n");
    fprintf(fp, "                                       (see also `-s`) (default: %lu)\n", 1000000000UL/opt_sleep_ns);
    fprintf(fp, "  -V, --php-version=<ver>            Set PHP version\n");
    fprintf(fp, "                                       (default: %s;\n", opt_phpv);
    fprintf(fp, "                                       supported: 70 71 72 73 74 80 81 82 83)\n");
    fprintf(fp, "  -l, --limit=<num>                  Limit total number of traces to capture\n");
    fprintf(fp, "                                       (approximate limit in pgrep mode)\n");
    fprintf(fp, "                                       (default: %lu; 0=unlimited)\n", opt_trace_limit);
    fprintf(fp, "  -i, --time-limit-ms=<ms>           Stop tracing after `ms` milliseconds\n");
    fprintf(fp, "                                       (second granularity in pgrep mode)\n");
    fprintf(fp, "                                       (default: %lu; 0=unlimited)\n", opt_time_limit_ms);
    fprintf(fp, "  -n, --max-depth=<max>              Set max stack trace depth\n");
    fprintf(fp, "                                       (default: %d; -1=unlimited)\n", opt_max_stack_depth);
    fprintf(fp, "  -r, --request-info=<opts>          Set request info parts to capture\n");
    fprintf(fp, "                                       (q=query c=cookie u=uri p=path\n");
    fprintf(fp, "                                       capital=negation)\n");
    fprintf(fp, "                                       (default: QCUP; none)\n");
    fprintf(fp, "  -m, --memory-usage                 Capture peak and current memory usage\n");
    fprintf(fp, "                                       with each trace (requires target PHP\n");
    fprintf(fp, "                                       process to have debug symbols)\n");
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
    fprintf(fp, "  -b, --buffer-size=<size>           Set output buffer size to `size`.\n");
    fprintf(fp, "                                       Note: In `-P` mode, setting this\n");
    fprintf(fp, "                                       above PIPE_BUF (4096) may lead to\n");
    fprintf(fp, "                                       interlaced writes across threads\n");
    fprintf(fp, "                                       unless `-J m` is specified.\n");
    fprintf(fp, "                                       (default: %d)\n", opt_fout_buffer_size);
    fprintf(fp, "  -f, --filter=<regex>               Filter output by POSIX regex\n");
    fprintf(fp, "                                       (default: none)\n");
    fprintf(fp, "  -F, --filter-negate=<regex>        Same as `-f` except negated\n");
    fprintf(fp, "  -d, --verbose-fields=<opts>        Set verbose output fields\n");
    fprintf(fp, "                                       (p=pid t=timestamp\n");
    fprintf(fp, "                                       capital=negation)\n");
    fprintf(fp, "                                       (default: PT; none)\n");
    fprintf(fp, "  -c, --continue-on-error            Attempt to continue tracing after\n");
    fprintf(fp, "                                       encountering an error\n");
    fprintf(fp, "  -#, --comment=<any>                Ignored; intended for self-documenting\n");
    fprintf(fp, "                                       commands\n");
    fprintf(fp, "  -@, --nothing                      Ignored\n");
    fprintf(fp, "  -v, --version                      Print phpspy version and exit\n");
    fprintf(fp, "\n");
    fprintf(fp, "Experimental options:\n");
    fprintf(fp, "  -j, --event-handler=<handler>      Set event handler (fout, callgrind)\n");
    fprintf(fp, "                                       (default: fout)\n");
    fprintf(fp, "  -J, --event-handler-opts=<opts>    Set event handler options\n");
    fprintf(fp, "                                       (fout: m=use mutex to prevent\n");
    fprintf(fp, "                                       interlaced writes on stdout in `-P`\n");
    fprintf(fp, "                                       mode)\n");
    fprintf(fp, "  -S, --pause-process                Pause process while reading stacktrace\n");
    fprintf(fp, "                                       (unsafe for production!)\n");
    fprintf(fp, "  -e, --peek-var=<varspec>           Peek at the contents of the var located\n");
    fprintf(fp, "                                       at `varspec`, which has the format:\n");
    fprintf(fp, "                                       <varname>@<path>:<lineno>\n");
    fprintf(fp, "                                       <varname>@<path>:<start>-<end>\n");
    fprintf(fp, "                                       e.g., xyz@/path/to.php:10-20\n");
    fprintf(fp, "  -g, --peek-global=<glospec>        Peek at the contents of a global var\n");
    fprintf(fp, "                                       located at `glospec`, which has\n");
    fprintf(fp, "                                       the format: <global>.<key>\n");
    fprintf(fp, "                                       where <global> is one of:\n");
    fprintf(fp, "                                       post|get|cookie|server|files|globals\n");
    fprintf(fp, "                                       e.g., server.REQUEST_TIME\n");
    fprintf(fp, "  -t, --top                          Show dynamic top-like output\n");
    cleanup();
    exit(exit_code);
}

static long strtol_with_min_or_exit(const char *name, const char *str, int min) {
    long result;
    char *end;
    errno = 0;
    result = strtol(str, &end, 10);
    if (end <= str || *end != '\0') {
        /* e.g. reject -H '', -H invalid, -H 5000suffix */
        log_error("Expected integer for %s, got '%s'\n", name, str);
        usage(stderr, 1);
    }
    if (result < min) {
        /* e.g. reject -H 0 */
        log_error("Expected integer >= %d for %s, got '%s'\n", min, name, str);
        usage(stderr, 1);
    }
    return result;
}

static int atoi_with_min_or_exit(const char *name, const char *str, int min) {
    long result = strtol_with_min_or_exit(name, str, min);
    #if LONG_MAX > INT_MAX
    if (result > INT_MAX) {
        log_error("Expected value that could fit in a C int for %s, got '%s'\n", name, str);
        usage(stderr, 1);
    }
    #endif
    return (int)result;
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
        { "time-limit-ms",         required_argument, NULL, 'i' },
        { "max-depth",             required_argument, NULL, 'n' },
        { "request-info",          required_argument, NULL, 'r' },
        { "memory-usage",          no_argument,       NULL, 'm' },
        { "output",                required_argument, NULL, 'o' },
        { "child-stdout",          required_argument, NULL, 'O' },
        { "child-stderr",          required_argument, NULL, 'E' },
        { "addr-executor-globals", required_argument, NULL, 'x' },
        { "addr-sapi-globals",     required_argument, NULL, 'a' },
        { "single-line",           no_argument,       NULL, '1' },
        { "buffer-size",           required_argument, NULL, 'b' },
        { "filter",                required_argument, NULL, 'f' },
        { "filter-negate",         required_argument, NULL, 'F' },
        { "verbose-fields",        required_argument, NULL, 'd' },
        { "continue-on-error",     no_argument,       NULL, 'c' },
        { "event-handler",         required_argument, NULL, 'j' },
        { "event-handler-opts",    required_argument, NULL, 'J' },
        { "comment",               required_argument, NULL, '#' },
        { "nothing",               no_argument,       NULL, '@' },
        { "version",               no_argument,       NULL, 'v' },
        { "pause-process",         no_argument,       NULL, 'S' },
        { "peek-var",              required_argument, NULL, 'e' },
        { "peek-global",           required_argument, NULL, 'g' },
        { "top",                   no_argument,       NULL, 't' },
        { 0,                       0,                 0,    0   }
    };
    /* Parse options until the first non-option argument is reached. Effectively
     * this makes the end-of-options-delimiter (`--`) optional. The following
     * invocations are equivalent:
     *   phpspy --rate-hz=2 php -r 'sleep(1);'
     *   phpspy --rate-hz=2 -- php -r 'sleep(1);'
     */
    optind = 1;
    while (
        optind < argc
        && argv[optind][0] == '-'
        && (c = getopt_long(argc, argv, "hp:P:T:te:s:H:V:l:i:n:r:mo:O:E:x:a:1b:f:F:d:cj:J:#:@vSe:g:t", long_opts, NULL)) != -1
    ) {
        switch (c) {
            case 'h': usage(stdout, 0); break;
            case 'p': opt_pid = atoi_with_min_or_exit("-p", optarg, 1); break;
            case 'P': opt_pgrep_args = optarg; break;
            case 'T': opt_num_workers = atoi_with_min_or_exit("-T", optarg, 1); break;
            case 's': opt_sleep_ns = strtol_with_min_or_exit("-s", optarg, 1); break;
            case 'H': opt_sleep_ns = (1000000000UL / strtol_with_min_or_exit("-H", optarg, 1)); break;
            case 'V': opt_phpv = optarg; break;
            case 'l': opt_trace_limit = strtoull(optarg, NULL, 10); break;
            case 'i': opt_time_limit_ms = strtol_with_min_or_exit("-i", optarg, 0); break;
            case 'n': opt_max_stack_depth = atoi_with_min_or_exit("-n", optarg, -1); break;
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
            case 'm': opt_capture_mem = 1; break;
            case 'o': opt_path_output = optarg; break;
            case 'O': opt_path_child_out = optarg; break;
            case 'E': opt_path_child_err = optarg; break;
            case 'x': opt_executor_globals_addr = strtoull(optarg, NULL, 16); break;
            case 'a': opt_sapi_globals_addr = strtoull(optarg, NULL, 16); break;
            case '1': opt_frame_delim = '\t'; opt_trace_delim = '\n'; break;
            case 'b': opt_fout_buffer_size = atoi_with_min_or_exit("-b", optarg, 1); break;
            case 'f':
            case 'F':
                if (opt_filter_re) {
                    regfree(opt_filter_re);
                }
                if (regcomp(&filter_re, optarg, REG_EXTENDED | REG_NOSUB | REG_NEWLINE) == 0) {
                    opt_filter_re = &filter_re;
                } else {
                    log_error("parse_opts: Failed to compile filter regex\n\n"); /* TODO regerror */
                    usage(stderr, 1);
                }
                opt_filter_negate = c == 'F' ? 1 : 0;
                break;
            case 'd':
                for (i = 0; i < strlen(optarg); i++) {
                    switch (optarg[i]) {
                        case 'p': opt_verbose_fields_pid = 1; break;
                        case 't': opt_verbose_fields_ts  = 1; break;
                        case 'P': opt_verbose_fields_pid = 0; break;
                        case 'T': opt_verbose_fields_ts  = 0; break;
                    }
                }
                break;
            case 'c': opt_continue_on_error = 1; break;
            case 'j':
                if (strcmp(optarg, "fout") == 0) {
                    opt_event_handler = event_handler_fout;
                } else if (strcmp(optarg, "callgrind") == 0) {
                    opt_event_handler = event_handler_callgrind;
                } else {
                    log_error("parse_opts: Expected 'fout' or 'callgrind' for `--event-handler`\n\n");
                    usage(stderr, 1);
                }
                break;
            case 'J':
                opt_event_handler_opts = optarg;
                break;
            case '#': break;
            case '@': break;
            case 'v':
                printf(
                    "phpspy v%s USE_ZEND=%s COMMIT=%s\n",
                    PHPSPY_VERSION,
                    #ifdef USE_ZEND
                    "y"
                    #else
                    "n"
                    #endif
                    ,
                    #ifdef COMMIT
                    STR2(COMMIT)
                    #else
                    "-"
                    #endif
                );
                exit(0);
            case 'S': opt_pause = 1; break;
            case 'e': varpeek_add(optarg); break;
            case 'g': glopeek_add(optarg); break;
            case 't': opt_top_mode = 1; break;
        }
    }
}

int main_pid(pid_t pid) {
    int rv;
    trace_context_t context;
    struct timespec start_time, end_time, sleep_time, _stop_time, limit_time;
    struct timespec *stop_time;

    memset(&context, 0, sizeof(trace_context_t));
    context.target.pid = pid;
    context.event_handler = opt_event_handler;
    context.event_handler_opts = opt_event_handler_opts;
    try(rv, find_addresses(&context.target));
    try(rv, context.event_handler(&context, PHPSPY_TRACE_EVENT_INIT));

    #ifdef USE_ZEND
    do_trace_ptr = do_trace;
    #else

    if (strcmp(opt_phpv, "auto") == 0) {
        try(rv, get_php_version(&context.target));
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
    } else if (strcmp("80", opt_phpv) == 0) {
        do_trace_ptr = do_trace_80;
    } else if (strcmp("81", opt_phpv) == 0) {
        do_trace_ptr = do_trace_81;
    } else if (strcmp("82", opt_phpv) == 0) {
        do_trace_ptr = do_trace_82;
    } else if (strcmp("83", opt_phpv) == 0) {
        do_trace_ptr = do_trace_83; /* TODO verify 8.3 structs */
    } else {
        log_error("main_pid: Unrecognized PHP version (%s)\n", opt_phpv);
        return PHPSPY_ERR;
    }
    #endif

    /* calc stop_time */
    stop_time = NULL;
    if (in_pgrep_mode) {
        /* in pgrep mode, we use a SIGALRM (see main_pgrep) */
    } else if (opt_time_limit_ms > 0) {
        stop_time = &_stop_time;
        limit_time.tv_sec = opt_time_limit_ms / 1000L;
        limit_time.tv_nsec = (opt_time_limit_ms % 1000L) * 1000000L;
        clock_get(stop_time);
        clock_add(stop_time, &limit_time, stop_time);
    }

    while (!done) {
        /* record start_time */
        clock_get(&start_time);

        /* trace (maybe with PTRACE_ATTACH) */
        rv = 0;
        if (opt_pause) rv |= pause_pid(pid);
        rv |= do_trace_ptr(&context);
        if (opt_pause) rv |= unpause_pid(pid);

        /* bail if pid died */
        if ((rv & PHPSPY_ERR_PID_DEAD) != 0) break;

        /* maybe apply trace limit */
        if (opt_trace_limit > 0 && rv == PHPSPY_OK) {
            if (in_pgrep_mode) {
                __atomic_add_fetch(&trace_count, 1, __ATOMIC_SEQ_CST);
            } else {
                trace_count += 1;
            }
            /* in pgrep mode, it is possible for each thread to sneak in one
               extra trace even after the limit is reached but it should exit
               after that. */
            if (trace_count >= opt_trace_limit) break;
        }

        /* maybe apply time limit */
        if (stop_time && clock_diff(&start_time, stop_time) >= 1) {
            break;
        }

        /* calc sleep_time and sleep */
        clock_get(&end_time);
        calc_sleep_time(&end_time, &start_time, &sleep_time);
        nanosleep(&sleep_time, NULL);
    }

    context.event_handler(&context, PHPSPY_TRACE_EVENT_DEINIT);

    /* in pgrep mode, trigger done condition if we went over the trace limit.
       it is ok for multiple threads to call this. */
    if (in_pgrep_mode && opt_trace_limit > 0 && trace_count >= opt_trace_limit) {
        write_done_pipe();
    }

    /* TODO proper signal handling for non-pgrep modes */
    return PHPSPY_OK;
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
        log_error("main_fork: Expected SIGTRAP from child\n");
    }
    ptrace(PTRACE_DETACH, fork_pid, NULL, NULL);
    rv = main_pid(fork_pid);
    waitpid(fork_pid, NULL, 0);
    return rv;
}

static void cleanup() {
    varpeek_entry_t *entry, *entry_tmp;
    varpeek_var_t *var, *var_tmp;
    glopeek_entry_t *gentry, *gentry_tmp;

    if (opt_filter_re) {
        regfree(opt_filter_re);
    }

    HASH_ITER(hh, varpeek_map, entry, entry_tmp) {
        HASH_ITER(hh, entry->varmap, var, var_tmp) {
            HASH_DEL(entry->varmap, var);
            free(var);
        }
        HASH_DEL(varpeek_map, entry);
        free(entry);
    }

    HASH_ITER(hh, glopeek_map, gentry, gentry_tmp) {
        HASH_DEL(glopeek_map, gentry);
        free(gentry);
    }
}

static int pause_pid(pid_t pid) {
    int rv;
    if (ptrace(PTRACE_ATTACH, pid, 0, 0) == -1) {
        rv = errno;
        perror("ptrace");
        return PHPSPY_ERR + (rv == ESRCH ? PHPSPY_ERR_PID_DEAD : 0);
    }
    if (waitpid(pid, NULL, 0) < 0) {
        perror("waitpid");
        return PHPSPY_ERR;
    }
    return PHPSPY_OK;
}

static int unpause_pid(pid_t pid) {
    int rv;
    if (ptrace(PTRACE_DETACH, pid, 0, 0) == -1) {
        rv = errno;
        perror("ptrace");
        return PHPSPY_ERR + (rv == ESRCH ? PHPSPY_ERR_PID_DEAD : 0);
    }
    return PHPSPY_OK;
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
    int rv;
    addr_memo_t memo;

    memset(&memo, 0, sizeof(addr_memo_t));

    if (opt_executor_globals_addr != 0) {
        target->executor_globals_addr = opt_executor_globals_addr;
    } else {
        try(rv, get_symbol_addr(&memo, target->pid, "executor_globals", &target->executor_globals_addr));
    }
    if (opt_sapi_globals_addr != 0) {
        target->sapi_globals_addr = opt_sapi_globals_addr;
    } else if (opt_capture_req) {
        try(rv, get_symbol_addr(&memo, target->pid, "sapi_globals", &target->sapi_globals_addr));
    }
    if (opt_capture_mem) {
        try(rv, get_symbol_addr(&memo, target->pid, "alloc_globals", &target->alloc_globals_addr));
    }
    log_error_enabled = 0;
    if (get_symbol_addr(&memo, target->pid, "basic_functions_module", &target->basic_functions_module_addr) != 0) {
        target->basic_functions_module_addr = 0;
    }
    log_error_enabled = 1;
    return PHPSPY_OK;
}

static void clock_get(struct timespec *ts) {
    if (clock_gettime(CLOCK_MONOTONIC_RAW, ts) == -1) {
        perror("clock_gettime");
        ts->tv_sec = 0;
        ts->tv_nsec = 0;
    }
}

static void clock_add(struct timespec *a, struct timespec *b, struct timespec *res) {
    res->tv_sec = a->tv_sec + b->tv_sec;
    res->tv_nsec = a->tv_nsec + b->tv_nsec;
    if (res->tv_nsec >= 1000000000L) {
        res->tv_sec += res->tv_nsec / 1000000000L;
        res->tv_nsec = res->tv_nsec % 1000000000L;
    }
}

static int clock_diff(struct timespec *a, struct timespec *b) {
    if (a->tv_sec == b->tv_sec) {
        if (a->tv_nsec == b->tv_nsec) {
            return 0;
        }
        return a->tv_nsec > b->tv_nsec ? 1 : -1;
    }
    return a->tv_sec > b->tv_sec ? 1 : -1;
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
        log_error("calc_sleep_time: Expected sleep_ns>0; decrease sample rate\n");
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
    char *at_sign, *colon, *dash;
    uint32_t line_start, line_end, lineno;
    varpeek_entry_t *varpeek;
    varpeek_var_t *var;
    char varpeek_key[PHPSPY_STR_SIZE];
    /* varspec: var@/path/file.php:line */
    /*   -or-   var@/path/file.php:start-end */
    at_sign = strchr(varspec, '@');
    colon = strrchr(varspec, ':');
    dash = strrchr(varspec, '-');
    if (!at_sign || !colon) {
        log_error("varpeek_add: Malformed varspec: %s\n\n", varspec);
        usage(stderr, 1);
    }
    line_start = strtoul(colon+1, NULL, 10);
    line_end = dash ? strtoul(dash+1, NULL, 10) : line_start;
    for (lineno = line_start; lineno <= line_end; lineno++) {
        snprintf(varpeek_key, sizeof(varpeek_key), "%.*s:%d", (int)(colon-at_sign-1), at_sign+1, lineno);
        HASH_FIND_STR(varpeek_map, varpeek_key, varpeek);
        if (!varpeek) {
            varpeek = calloc(1, sizeof(varpeek_entry_t));
            strncpy(varpeek->filename_lineno, varpeek_key, sizeof(varpeek->filename_lineno));
            HASH_ADD_STR(varpeek_map, filename_lineno, varpeek);
        }
        var = calloc(1, sizeof(varpeek_var_t));
        snprintf(var->name, sizeof(var->name), "%.*s", (int)(at_sign-varspec), varspec);
        HASH_ADD_STR(varpeek->varmap, name, var);
    }
}

static void glopeek_add(char *glospec) {
    char *dot;
    char *gloname;
    glopeek_entry_t *gentry;
    dot = strchr(glospec, '.');
    if (!dot) {
        log_error("glopeek_add: Malformed glospec: %s\n\n", glospec);
        usage(stderr, 1);
    }
    HASH_FIND_STR(glopeek_map, glospec, gentry);
    if (gentry) return;
    if (strncmp("post.", glospec, dot-glospec) == 0) {
        gloname = "_POST";
    } else if (strncmp("get.", glospec, dot-glospec) == 0) {
        gloname = "_GET";
    } else if (strncmp("cookie.", glospec, dot-glospec) == 0) {
        gloname = "_COOKIE";
    } else if (strncmp("server.", glospec, dot-glospec) == 0) {
        gloname = "_SERVER";
    } else if (strncmp("files.", glospec, dot-glospec) == 0) {
        gloname = "_FILES";
    } else if (strncmp("globals.", glospec, dot-glospec) == 0) {
        gloname = ""; /* $GLOBAL variables are stored directly in the symbol table */
    } else {
        log_error("glopeek_add: Invalid global: %s\n\n", glospec);
        usage(stderr, 1);
        return;
    }
    gentry = calloc(1, sizeof(glopeek_entry_t));
    snprintf(gentry->key, sizeof(gentry->key), "%s", glospec);
    snprintf(gentry->gloname, sizeof(gentry->gloname), "%s", gloname);
    snprintf(gentry->varname, sizeof(gentry->varname), "%s", dot+1);
    HASH_ADD_STR(glopeek_map, key, gentry);
}

static int copy_proc_mem(pid_t pid, const char *what, void *raddr, void *laddr, size_t size) {
    struct iovec local[1];
    struct iovec remote[1];

    if (raddr == NULL) {
        log_error("copy_proc_mem: Not copying %s; raddr is NULL\n", what);
        return PHPSPY_ERR;
    }

    local[0].iov_base = laddr;
    local[0].iov_len = size;
    remote[0].iov_base = raddr;
    remote[0].iov_len = size;

    if (process_vm_readv(pid, local, 1, remote, 1, 0) == -1) {
        if (errno == ESRCH) { /* No such process */
            perror("process_vm_readv");
            return PHPSPY_ERR | PHPSPY_ERR_PID_DEAD;
        }
        log_error("copy_proc_mem: Failed to copy %s; err=%s raddr=%p size=%lu\n", what, strerror(errno), raddr, size);
        return PHPSPY_ERR;
    }

    return PHPSPY_OK;
}

#ifndef USE_ZEND
static int get_php_version(trace_target_t *target) {
    struct _zend_module_entry basic_functions_module;
    char version_cmd[1024];
    char phpv[4];
    pid_t pid;
    FILE *pcmd;

    pid = target->pid;
    phpv[0] = '\0';

    /* Try reading basic_functions_module */
    if (target->basic_functions_module_addr) {
        if (copy_proc_mem(pid, "basic_functions_module", (void*)target->basic_functions_module_addr, &basic_functions_module, sizeof(basic_functions_module)) == 0) {
            copy_proc_mem(pid, "basic_functions_module.version", (void*)basic_functions_module.version, phpv, 3);
        }
    }

    /* Try greping binary */
    if (phpv[0] == '\0') {
        int n = snprintf(
            version_cmd,
            sizeof(version_cmd),
            "{ echo -n /proc/%d/root/; "
            "  awk -ve=1 '/libphp[78]?/{print $NF; e=0; exit} END{exit e}' /proc/%d/maps "
            "  || readlink /proc/%d/exe; } "
            "| { xargs stat --printf=%%n 2>/dev/null || echo /proc/%d/exe; } "
            "| xargs strings "
            "| grep -Po '(?<=X-Powered-By: PHP/)\\d\\.\\d'",
            pid, pid, pid, pid
        );
        if ((size_t)n >= sizeof(version_cmd) - 1) {
            log_error("get_php_version: snprintf overflow\n");
            return PHPSPY_ERR;
        }

        if ((pcmd = popen(version_cmd, "r")) == NULL) {
            perror("get_php_version: popen");
            return PHPSPY_ERR;
        } else if (fread(&phpv, sizeof(char), 3, pcmd) != 3) {
            log_error("get_php_version: Could not detect PHP version\n");
            pclose(pcmd);
            return PHPSPY_ERR;
        }
        pclose(pcmd);
    }

    if      (strncmp(phpv, "7.0", 3) == 0) opt_phpv = "70";
    else if (strncmp(phpv, "7.1", 3) == 0) opt_phpv = "71";
    else if (strncmp(phpv, "7.2", 3) == 0) opt_phpv = "72";
    else if (strncmp(phpv, "7.3", 3) == 0) opt_phpv = "73";
    else if (strncmp(phpv, "7.4", 3) == 0) opt_phpv = "74";
    else if (strncmp(phpv, "8.0", 3) == 0) opt_phpv = "80";
    else if (strncmp(phpv, "8.1", 3) == 0) opt_phpv = "81";
    else if (strncmp(phpv, "8.2", 3) == 0) opt_phpv = "82";
    else if (strncmp(phpv, "8.3", 3) == 0) opt_phpv = "83";
    else {
        log_error("get_php_version: Unrecognized PHP version (%s)\n", phpv);
        return PHPSPY_ERR;
    }

    return PHPSPY_OK;
}
#endif

uint64_t phpspy_zend_inline_hash_func(const char *str, size_t len) {
    /* Adapted from zend_string.h */
    uint64_t hash;
    hash = 5381UL;

    for (; len >= 8; len -= 8) {
        hash = ((hash << 5) + hash) + *str++;
        hash = ((hash << 5) + hash) + *str++;
        hash = ((hash << 5) + hash) + *str++;
        hash = ((hash << 5) + hash) + *str++;
        hash = ((hash << 5) + hash) + *str++;
        hash = ((hash << 5) + hash) + *str++;
        hash = ((hash << 5) + hash) + *str++;
        hash = ((hash << 5) + hash) + *str++;
    }
    switch (len) {
        case 7: hash = ((hash << 5) + hash) + *str++; /* fallthrough... */
        case 6: hash = ((hash << 5) + hash) + *str++; /* fallthrough... */
        case 5: hash = ((hash << 5) + hash) + *str++; /* fallthrough... */
        case 4: hash = ((hash << 5) + hash) + *str++; /* fallthrough... */
        case 3: hash = ((hash << 5) + hash) + *str++; /* fallthrough... */
        case 2: hash = ((hash << 5) + hash) + *str++; /* fallthrough... */
        case 1: hash = ((hash << 5) + hash) + *str++; break;
        case 0: break;
    }

    return hash | 0x8000000000000000UL;
}

void log_error(const char *fmt, ...) {
    va_list args;
    if (log_error_enabled) {
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
    }
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
#define phpv 80
#include "phpspy_trace_tpl.c"
#undef phpv
#define phpv 81
#include "phpspy_trace_tpl.c"
#undef phpv
#define phpv 82
#include "phpspy_trace_tpl.c"
#undef phpv
#define phpv 83
#include "phpspy_trace_tpl.c"
#undef phpv
#endif

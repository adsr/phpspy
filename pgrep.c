#include "phpspy.h"

#define PHPSPY_MAX_SELF_PIDS 32

/* How long a pid we failed to attach to is left alone before trying again.
   Bounded rather than permanent so a recycled pid, or one that becomes
   traceable later, is eventually picked up. */
#define PHPSPY_BAD_PID_RETRY_S 30

/* Two strikes before sidelining a pid: address resolution legitimately fails
   for a process caught between fork and exec. */
#define PHPSPY_BAD_PID_STRIKES 2

typedef struct bad_pid_s {
    int pid;
    time_t when;
    int fails;
    UT_hash_handle hh;
} bad_pid_t;

static int wait_for_turn(char producer_or_consumer);
static void pgrep_for_pids();
static void *run_work_thread(void *arg);
static int is_already_attached(int pid);
static void init_work_threads();
static void deinit_work_threads();
static int block_all_signals();
static void handle_signal(int signum);
static void *run_signal_thread(void *arg);
static pid_t get_ppid(pid_t pid);
static void collect_self_pids();
static int is_self_pid(pid_t pid);
static int pid_looks_like_php(pid_t pid);
static int is_bad_pid(int pid);
static void mark_bad_pid(int pid);
static void free_bad_pids();

static int *avail_pids = NULL;
static int *attached_pids = NULL;
static pthread_t *work_threads = NULL;
static pthread_t signal_thread;
static int avail_pids_count = 0;
static int pgrep_failed = 0;
static pid_t self_pids[PHPSPY_MAX_SELF_PIDS];
static int self_pids_len = 0;
static regex_t libname_re;
static int libname_re_ok = 0;
static bad_pid_t *bad_pids = NULL;
static pthread_mutex_t bad_pids_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t can_produce = PTHREAD_COND_INITIALIZER;
static pthread_cond_t can_consume = PTHREAD_COND_INITIALIZER;
static int done_pipe[2] = { -1, -1 };

int main_pgrep() {
    long i;

    if (opt_num_workers < 1) {
        log_error("Expected max concurrent workers (-T) > 0\n");
        exit(1);
    }

    pthread_create(&signal_thread, NULL, run_signal_thread, NULL);
    block_all_signals();

    collect_self_pids();
    /* awk patterns are EREs, so `-w` works here unchanged */
    libname_re_ok = regcomp(&libname_re, opt_libname_awk_patt, REG_EXTENDED | REG_NOSUB) == 0 ? 1 : 0;
    if (!libname_re_ok) {
        log_error("main_pgrep: Failed to compile -w pattern; not filtering non-PHP pids by maps\n");
    }

    init_work_threads();

    for (i = 0; i < opt_num_workers; i++) {
        pthread_create(&work_threads[i], NULL, run_work_thread, (void*)i);
    }

    if (opt_time_limit_ms > 0) {
        alarm(PHPSPY_MAX(1, opt_time_limit_ms / 1000));
    }

    pgrep_for_pids();

    for (i = 0; i < opt_num_workers; i++) {
        pthread_join(work_threads[i], NULL);
    }
    pthread_join(signal_thread, NULL);

    deinit_work_threads();

    if (libname_re_ok) {
        regfree(&libname_re);
    }
    free_bad_pids();

    log_error("main_pgrep finished gracefully\n");
    return pgrep_failed ? PHPSPY_ERR : 0;
}

static int wait_for_turn(char producer_or_consumer) {
    struct timespec timeout;
    pthread_mutex_lock(&mutex);
    while (!done) {
        if (producer_or_consumer == 'p' && avail_pids_count < opt_num_workers) {
            break;
        } else if (avail_pids_count > 0) {
            break;
        }
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 2;
        pthread_cond_timedwait(
            producer_or_consumer == 'p' ? &can_produce : &can_consume,
            &mutex,
            &timeout
        );
    }
    if (done) {
        pthread_mutex_unlock(&mutex);
        return 1;
    }
    return 0;
}

static void pgrep_for_pids() {
    FILE *pcmd;
    char *pgrep_cmd;
    char line[64];
    int pid;
    int found;
    int wstatus, ec;
    struct timespec timeout;
    if (asprintf(&pgrep_cmd, "pgrep %s%s", opt_pgrep_args, opt_quiet ? " 2>/dev/null" : "") < 0) {
        errno = ENOMEM;
        log_perror("asprintf");
        exit(1);
    }
    while (!done) {
        if (wait_for_turn('p')) break;
        found = 0;
        if ((pcmd = popen(pgrep_cmd, "r")) == NULL) {
            log_perror("pgrep_for_pids: popen");
            pgrep_failed = 1;
        } else {
            while (avail_pids_count < opt_num_workers && fgets(line, sizeof(line), pcmd) != NULL) {
                if (strlen(line) < 1 || *line == '\n') continue;
                pid = atoi(line);
                if (pid < 1) continue;
                if (is_self_pid(pid)) continue;
                if (is_already_attached(pid)) continue;
                if (is_bad_pid(pid)) continue;
                if (!pid_looks_like_php(pid)) continue;
                avail_pids[avail_pids_count++] = pid;
                found += 1;
            }
            wstatus = pclose(pcmd);
            ec = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : -1;
            /* pgrep: 0=matched, 1=no match, 2=syntax error, 3=fatal. Only a
               real error is fatal here; "no match" is the normal idle case. */
            if (ec >= 2 || ec < 0) {
                log_error(
                    "pgrep_for_pids: `%s` exited with status %d; check your -P arguments\n"
                    "        (the argument string is word-split, so a pattern containing spaces\n"
                    "         becomes several pgrep patterns)\n",
                    pgrep_cmd,
                    ec
                );
                pgrep_failed = 1;
            }
        }
        if (found > 0) {
            pthread_cond_broadcast(&can_consume);
        } else {
            clock_gettime(CLOCK_REALTIME, &timeout);
            timeout.tv_sec += 2;
            pthread_cond_timedwait(
                &can_produce,
                &mutex,
                &timeout
            );
        }
        pthread_mutex_unlock(&mutex);
        if (pgrep_failed) {
            /* a bad -P never fixes itself; stop rather than looping silently */
            write_done_pipe();
            break;
        }
    }
    free(pgrep_cmd);
}

static void *run_work_thread(void *arg) {
    int worker_num, pid, rv;
    worker_num = (long)arg;
    while (!done) {
        if (wait_for_turn('c')) break;
        attached_pids[worker_num] = avail_pids[--avail_pids_count];
        pthread_cond_signal(&can_produce);
        pthread_mutex_unlock(&mutex);
        pid = attached_pids[worker_num];
        rv = main_pid(pid);
        /* main_pid only fails before it starts sampling, i.e. we could not
           attach at all; remember that so the producer stops handing this pid
           back to us on every poll */
        if (rv != PHPSPY_OK) {
            mark_bad_pid(pid);
        }
        attached_pids[worker_num] = 0;
    }
    return NULL;
}

static pid_t get_ppid(pid_t pid) {
    char path[PHPSPY_STR_SIZE];
    char line[PHPSPY_STR_SIZE];
    FILE *fp;
    pid_t ppid;

    ppid = 0;
    snprintf(path, sizeof(path), "/proc/%d/status", (int)pid);
    if ((fp = fopen(path, "r")) == NULL) {
        return 0;
    }
    /* status(5) rather than stat(5): the latter's comm field can contain
       spaces and parens, which makes positional parsing unreliable */
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "PPid:", 5) == 0) {
            ppid = (pid_t)atoi(line + 5);
            break;
        }
    }
    fclose(fp);

    return ppid;
}

static void collect_self_pids() {
    pid_t pid;

    /* A `-P` pattern routinely matches phpspy's own command line, and the
       shell or sudo that launched it, since the pattern text appears there.
       Threads share the tgid, so getpid() covers every worker. */
    self_pids_len = 0;
    pid = getpid();
    while (pid > 1 && self_pids_len < PHPSPY_MAX_SELF_PIDS) {
        self_pids[self_pids_len++] = pid;
        pid = get_ppid(pid);
    }
}

static int is_self_pid(pid_t pid) {
    int i;
    for (i = 0; i < self_pids_len; i++) {
        if (self_pids[i] == pid) return 1;
    }
    return 0;
}

static int pid_looks_like_php(pid_t pid) {
    char path[PHPSPY_STR_SIZE];
    char exe[PHPSPY_STR_SIZE];
    char line[PHPSPY_STR_SIZE];
    ssize_t len;
    FILE *fp;
    int found;

    /* Cheap gate in front of find_addresses, which otherwise spends four
       popen'd shell commands per poll discovering that bash is not PHP.
       Deliberately fails open: a false negative would silently drop a real
       target, so anything we cannot determine is allowed through. */
    snprintf(path, sizeof(path), "/proc/%d/exe", (int)pid);
    len = readlink(path, exe, sizeof(exe) - 1);
    if (len < 0) return 1; /* not permitted to look, or already gone */
    exe[len] = '\0';
    if (strstr(exe, "php") != NULL) return 1;
    if (!libname_re_ok) return 1;

    /* mod_php and friends run under another exe name, so look for the lib */
    snprintf(path, sizeof(path), "/proc/%d/maps", (int)pid);
    if ((fp = fopen(path, "r")) == NULL) return 1;
    found = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (regexec(&libname_re, line, 0, NULL, 0) == 0) {
            found = 1;
            break;
        }
    }
    fclose(fp);

    return found;
}

static int is_bad_pid(int pid) {
    bad_pid_t *bad;
    int stale;

    /* Without this, a pid we cannot attach to -- another user's PHP process,
       say, whose /proc is not readable -- is re-queued on every poll, and each
       attempt spends several popen'd shell commands rediscovering that. */
    pthread_mutex_lock(&bad_pids_mutex);
    HASH_FIND_INT(bad_pids, &pid, bad);
    stale = bad != NULL && (time(NULL) - bad->when) >= PHPSPY_BAD_PID_RETRY_S;
    if (stale) {
        HASH_DEL(bad_pids, bad);
        free(bad);
        bad = NULL;
    }
    pthread_mutex_unlock(&bad_pids_mutex);

    return (bad != NULL && bad->fails >= PHPSPY_BAD_PID_STRIKES) ? 1 : 0;
}

static void mark_bad_pid(int pid) {
    bad_pid_t *bad;

    pthread_mutex_lock(&bad_pids_mutex);
    HASH_FIND_INT(bad_pids, &pid, bad);
    if (bad == NULL) {
        if ((bad = calloc(1, sizeof(bad_pid_t))) != NULL) {
            bad->pid = pid;
            HASH_ADD_INT(bad_pids, pid, bad);
        }
    }
    if (bad != NULL) {
        bad->when = time(NULL);
        bad->fails += 1;
    }
    pthread_mutex_unlock(&bad_pids_mutex);
}

static void free_bad_pids() {
    bad_pid_t *bad, *bad_tmp;
    HASH_ITER(hh, bad_pids, bad, bad_tmp) {
        HASH_DEL(bad_pids, bad);
        free(bad);
    }
}

static int is_already_attached(int pid) {
    int i;
    for (i = 0; i < opt_num_workers; i++) {
        if (attached_pids[i] == pid) {
            return 1;
        } else if (i < avail_pids_count && avail_pids[i] == pid) {
            return 1;
        }
    }
    return 0;
}

static void init_work_threads() {
    avail_pids = calloc(opt_num_workers, sizeof(int));
    attached_pids = calloc(opt_num_workers, sizeof(int));
    work_threads = calloc(opt_num_workers, sizeof(pthread_t));
    if (!avail_pids || !attached_pids || !work_threads) {
        errno = ENOMEM;
        log_perror("calloc");
        exit(1);
    }
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&can_produce, NULL);
    pthread_cond_init(&can_consume, NULL);
}

static void deinit_work_threads() {
    free(avail_pids);
    free(attached_pids);
    free(work_threads);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&can_produce);
    pthread_cond_destroy(&can_consume);
}

static int block_all_signals() {
    int rv;
    sigset_t set;
    try(rv, sigfillset(&set));
    try(rv, sigprocmask(SIG_BLOCK, &set, NULL));
    return 0;
}

void write_done_pipe() {
    int rv, ignore;
    if (done_pipe[1] >= 0) {
        ignore = 1;
        rv = write(done_pipe[1], &ignore, sizeof(int));
    }
    (void)rv;
}

static void handle_signal(int signum) {
    (void)signum;
    write_done_pipe();
}

static void *run_signal_thread(void *arg) {
    int rv, ignore;
    fd_set rfds;
    struct timeval tv;
    struct sigaction sa;

    (void)arg;

    /* Create done_pipe */
    rv = pipe(done_pipe);
    rv = fcntl(done_pipe[1], F_SETFL, O_NONBLOCK);

    /* Install signal handler */
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGALRM, &sa, NULL);
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);

    /* Wait for write on done_pipe from write_done_pipe */
    do {
        FD_ZERO(&rfds);
        FD_SET(done_pipe[0], &rfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        rv = select(done_pipe[0]+1, &rfds, NULL, NULL, &tv);
    } while (rv < 1);

    /* Read pipe for fun */
    rv = read(done_pipe[0], &ignore, sizeof(int));

    /* Set done flag; wake up all threads */
    done = 1;
    pthread_mutex_lock(&mutex);
    pthread_cond_broadcast(&can_consume);
    pthread_cond_broadcast(&can_produce);
    pthread_mutex_unlock(&mutex);

    return NULL;
}

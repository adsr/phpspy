#include "phpspy.h"

static int wait_for_turn(char producer_or_consumer);
static void pgrep_for_pids();
static void *run_work_thread(void *arg);
static int is_already_attached(int pid);
static void init_work_threads();
static void deinit_work_threads();
static int block_all_signals();
static void handle_signal(int signum);
static void *run_signal_thread(void *arg);

static int *avail_pids = NULL;
static int *attached_pids = NULL;
static pthread_t *work_threads = NULL;
static pthread_t signal_thread;
static int avail_pids_count = 0;
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

    init_work_threads();

    for (i = 0; i < opt_num_workers; i++) {
        pthread_create(&work_threads[i], NULL, run_work_thread, (void*)i);
    }

    pgrep_for_pids();

    for (i = 0; i < opt_num_workers; i++) {
        pthread_join(work_threads[i], NULL);
    }
    pthread_join(signal_thread, NULL);

    deinit_work_threads();

    log_error("main_pgrep finished gracefully\n");
    return 0;
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
    struct timespec timeout;
    if (asprintf(&pgrep_cmd, "pgrep %s", opt_pgrep_args) < 0) {
        errno = ENOMEM;
        perror("asprintf");
        exit(1);
    }
    while (!done) {
        if (wait_for_turn('p')) break;
        found = 0;
        if ((pcmd = popen(pgrep_cmd, "r")) != NULL) {
            while (avail_pids_count < opt_num_workers && fgets(line, sizeof(line), pcmd) != NULL) {
                if (strlen(line) < 1 || *line == '\n') continue;
                pid = atoi(line);
                if (is_already_attached(pid)) continue;
                avail_pids[avail_pids_count++] = pid;
                found += 1;
            }
            pclose(pcmd);
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
    }
    free(pgrep_cmd);
}

static void *run_work_thread(void *arg) {
    int worker_num;
    worker_num = (long)arg;
    while (!done) {
        if (wait_for_turn('c')) break;
        attached_pids[worker_num] = avail_pids[--avail_pids_count];
        pthread_cond_signal(&can_produce);
        pthread_mutex_unlock(&mutex);
        main_pid(attached_pids[worker_num]);
        attached_pids[worker_num] = 0;
    }
    return NULL;
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
        perror("calloc");
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

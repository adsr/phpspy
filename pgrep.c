#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <pthread.h>

extern int main_pid();
extern int opt_num_workers;
extern char *opt_pgrep_args;
extern int done;

static int wait_for_turn(char producer_or_consumer);
static void pgrep_for_pids();
static void *work(void *arg);
static int is_already_attached(int pid);
static void init_threads();
static void init_signal_handler();
static void handle_signal(int signum);

int *avail_pids = NULL;
int *attached_pids = NULL;
pthread_t *work_thread = NULL;
int avail_pids_count = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t can_produce = PTHREAD_COND_INITIALIZER;
pthread_cond_t can_consume = PTHREAD_COND_INITIALIZER;

int main_pgrep() {
    long i;

    if (opt_num_workers < 1) {
        fprintf(stderr, "Expected max concurrent workers (-N) > 0\n");
        exit(1);
    }

    init_signal_handler();
    init_threads();

    for (i = 0; i < opt_num_workers; i++) {
        pthread_create(&work_thread[i], NULL, work, (void*)i);
        // pthread_detach(work_thread[i]);
    }

    pgrep_for_pids();

    for (i = 0; i < opt_num_workers; i++) {
        pthread_join(work_thread[i], NULL);
    }

    free(avail_pids);
    free(attached_pids);
    free(work_thread);

    fprintf(stderr, "main_pgrep finished gracefully\n");
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
    if (asprintf(&pgrep_cmd, "pgrep %s", opt_pgrep_args) < 0) {
        errno = ENOMEM;
        perror("asprintf");
        exit(1);
    }
    while (!done) {
        if (wait_for_turn('p')) break;
        pcmd = popen(pgrep_cmd, "r");
        found = 0;
        while (avail_pids_count < opt_num_workers && fgets(line, sizeof(line), pcmd) != NULL) {
            if (strlen(line) < 1 || *line == '\n') continue;
            pid = atoi(line);
            if (is_already_attached(pid)) continue;
            avail_pids[avail_pids_count++] = pid;
            found += 1;
        }
        pclose(pcmd);
        if (found > 0) pthread_cond_broadcast(&can_consume);
        pthread_mutex_unlock(&mutex);
        if (found < 1) sleep(2);
    }
    free(pgrep_cmd);
}

static void *work(void *arg) {
    int worker_num;
    worker_num = (long)arg;
    while (!done) {
        if (wait_for_turn('c')) break;
        attached_pids[worker_num] = avail_pids[--avail_pids_count];
        pthread_cond_signal(&can_produce);
        pthread_mutex_unlock(&mutex);
        printf("worker_num %d in\n", worker_num);
        main_pid(attached_pids[worker_num]);
        printf("worker_num %d out\n", worker_num);
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

static void init_threads() {
    avail_pids = calloc(opt_num_workers, sizeof(int));
    attached_pids = calloc(opt_num_workers, sizeof(int));
    work_thread = calloc(opt_num_workers, sizeof(pthread_t));
    if (!avail_pids || !attached_pids || !work_thread) {
        errno = ENOMEM;
        perror("calloc");
        exit(1);
    }
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&can_produce, NULL);
    pthread_cond_init(&can_consume, NULL);
}

static void init_signal_handler() {
    // TODO Tighten up signal handling
    struct sigaction sa = {0};

    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);
}

static void handle_signal(int signum) {
    //int w;
    (void)signum;
    //w = write(STDERR_FILENO, "\nmain_pgrep caught signal\n", sizeof("\nmain_pgrep caught signal\n"));
    done = 1;
}

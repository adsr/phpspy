#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termbox.h>
#include <time.h>
#include <unistd.h>
#include "uthash.h"

extern void usage(FILE *fp, int exit_code);
extern int done;
extern pid_t opt_pid;
extern char *opt_pgrep_args;

#define PHPSPY_MAX(a, b) ((a) > (b) ? (a) : (b))
#define FUNC_SIZE 256
#define BUF_SIZE 512

struct func_t {
    char func[FUNC_SIZE];
    unsigned long long count_own;
    unsigned long long count_total;
    // TODO figure out rate using sample rate from child + refresh rate
    size_t index;
    UT_hash_handle hh;
};

static int fork_child(int argc, char **argv, pid_t *pid, int *outfd, int *errfd);
static void filter_child_args(int argc, char **argv);
static void read_child_out(int fd);
static void read_child_err(int fd);
static void handle_line(char *line, int line_len);
static void handle_event(struct tb_event *event);
static void display();
static void tb_print(const char *str, int x, int y, uint16_t fg, uint16_t bg);
static void tb_printf(int x, int y, uint16_t fg, uint16_t bg, const char *fmt, ...);

struct func_t *func_map = NULL;
struct func_t **func_list = NULL;
size_t func_list_len = 0;
size_t func_list_size = 0;
char buf[BUF_SIZE];
size_t buf_len = 0;
unsigned long long samp_count = 0;
unsigned long long err_count = 0;
char phpspy_args[BUF_SIZE];

int main_top(int argc, char **argv) {
    fd_set readfds;
    int ttyfd, outfd, errfd, maxfd;
    pid_t pid;
    struct timeval timeout;
    struct timespec ts, last_display;
    struct tb_event event;
    int rc, i;

    outfd = errfd = ttyfd = pid = -1;

    if (opt_pid == -1 && opt_pgrep_args == NULL && optind >= argc) {
        fprintf(stderr, "Expected pid (-p), pgrep (-P), or command\n\n");
        usage(stderr, 1);
        return 1;
    }

    filter_child_args(argc, argv);
    snprintf(phpspy_args, BUF_SIZE, "phpspy ");
    for (i = 1; i < argc; i++) {
        snprintf(phpspy_args+strlen(phpspy_args), BUF_SIZE-strlen(phpspy_args), "%s ", argv[i]);
    }

    if ((ttyfd = open("/dev/tty", O_RDONLY)) < 0) {
        perror("open");
        return 1;
    }
    if ((fork_child(argc, argv, &pid, &outfd, &errfd)) < 0) {
        return 1;
    }
    maxfd = PHPSPY_MAX(PHPSPY_MAX(ttyfd, outfd), errfd);

    last_display.tv_sec = 0;
    last_display.tv_nsec = 0;

    tb_init();
    while (!done) {
        FD_ZERO(&readfds);
        FD_SET(ttyfd, &readfds);
        FD_SET(outfd, &readfds);
        FD_SET(errfd, &readfds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        rc = select(maxfd + 1, &readfds, NULL, NULL, &timeout);
        if (rc < 0) {
            perror("select");
            break;
        }
        if (FD_ISSET(outfd, &readfds)) {
            read_child_out(outfd);
        }
        if (FD_ISSET(errfd, &readfds)) {
            read_child_err(errfd);
        }
        if (FD_ISSET(ttyfd, &readfds)) {
            event.type = 0;
            tb_peek_event(&event, 0);
            handle_event(&event);
        }
        clock_gettime(CLOCK_MONOTONIC, &ts);
        if (ts.tv_sec - last_display.tv_sec >= 1) {
            display();
            last_display = ts;
        }
    }
    tb_shutdown();

    close(outfd);
    close(errfd);
    close(ttyfd);
    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
    return 0;
}

static int fork_child(int argc, char **argv, pid_t *pid, int *outfd, int *errfd) {
    int pout[2], perr[2];
    (void)argc;
    if (pipe(pout) < 0 || pipe(perr) < 0) {
        perror("pipe");
        return 1;
    }
    *pid = fork();
    if (*pid == 0) {
        close(pout[0]);
        dup2(pout[1], STDOUT_FILENO);
        close(pout[1]);

        close(perr[0]);
        dup2(perr[1], STDERR_FILENO);
        close(perr[1]);

        execvp(argv[0], argv);
        perror("execvp");
        exit(1);
    } else if (*pid < 0) {
        perror("fork");
        return 1;
    }
    close(pout[1]);
    close(perr[1]);
    *outfd = pout[0];
    *errfd = perr[0];
    return 0;
}

static void filter_child_args(int argc, char **argv) {
    int i;
    for (i = 0; i < argc; i++) {
        if (strncmp(argv[i], "-o",            sizeof("-o")-1)            == 0
         || strncmp(argv[i], "--output",      sizeof("--output")-1)      == 0
        ) {
            argv[i][1] = '#';
            argv[i][2] = '\0';
        }
        if (strncmp(argv[i], "-1",            sizeof("-1")-1)            == 0
         || strncmp(argv[i], "--single-line", sizeof("--single-line")-1) == 0
         || strncmp(argv[i], "-t",            sizeof("-t")-1)            == 0
         || strncmp(argv[i], "--top",         sizeof("--top")-1)         == 0
        ) {
            argv[i][1] = '@';
            argv[i][2] = '\0';
        }
    }
}

static void read_child_out(int fd) {
    size_t rem, line_len;
    char *nl;

    ssize_t read_rv;
    rem = BUF_SIZE - buf_len;
    if (rem < 1) {
        buf_len = 0;
        rem = BUF_SIZE;
    }

    if ((read_rv = read(fd, buf + buf_len, rem)) < 0) {
        perror("read");
        return;
    } else if (read_rv == 0) {
        done = 1;
        return;
    }
    buf_len += read_rv;

    while ((nl = memchr(buf, '\n', buf_len)) != NULL) {
        line_len = nl-buf;
        handle_line(buf, line_len);
        memmove(buf, nl+1, buf_len-line_len+1);
        buf_len -= line_len+1;
    }
}

static void read_child_err(int fd) {
    char buf[BUF_SIZE];
    char *nl;
    size_t buf_pos;
    ssize_t read_rv;
    if ((read_rv = read(fd, buf, BUF_SIZE)) < 0) {
        perror("read");
        return;
    } else if (read_rv == 0) {
        done = 1;
        return;
    }
    buf_pos = 0;
    while (read_rv > 0 && (nl = memchr(buf+buf_pos, '\n', read_rv)) != NULL) {
        err_count += 1;
        buf_pos += nl-buf+1;
        read_rv -= nl-buf+1;
    }
}

static void handle_line(char *line, int line_len) {
    unsigned long long frame_num;
    char *func;
    size_t func_len;
    struct func_t *func_el;
    size_t i, j, k;

    //printf("line=[%.*s]\n", line_len, line);

    if (line_len < 3) return;
    if (line[0] == '#') return;
    frame_num = strtoull(line, &func, 10);
    if (frame_num == 0 && line[0] != '0') return;
    if (*func != ' ') return;
    func += 1;
    func_len = line_len-(func-line);
    if (func_len < 1 || func_len >= FUNC_SIZE) return;

    HASH_FIND(hh, func_map, func, func_len, func_el);
    if (!func_el) {
        func_el = calloc(1, sizeof(struct func_t));
        snprintf(func_el->func, FUNC_SIZE, "%.*s", (int)func_len, func);
        HASH_ADD_STR(func_map, func, func_el);
        func_list_len += 1;
        if (func_list_len > func_list_size) {
            func_list = realloc(func_list, sizeof(struct func_t*) * (func_list_size + 1024));
            func_list_size += 1024;
        }
        func_list[func_list_len-1] = func_el;
        func_el->index = func_list_len-1;
    }

    if (frame_num == 0) {
        samp_count += 1;
        func_el->count_own += 1;
    }
    func_el->count_total += 1;

    i = func_el->index;
    while (i > 0 && func_el->count_own > func_list[i-1]->count_own) {
        i -= 1;
    }
    if (i < func_el->index) {
        memmove(
            func_list + i + 1,
            func_list + i,
            sizeof(struct func_t*) * (func_el->index-i)
        );
        func_list[i] = func_el;
        k = func_el->index;
        for (j = i; j <= k; j++) {
            func_list[j]->index = j;
        }
    }
}

static void handle_event(struct tb_event *event) {
    size_t i;
    if (event->type != TB_EVENT_KEY) {
        return;
    } else if (event->ch == 'q') {
        done = 1;
    } else if (event->ch == 'c') {
        for (i = 0; i < func_list_len; i++) {
            func_list[i]->count_own = 0;
            func_list[i]->count_total = 0;
        }
        samp_count = 0;
        err_count = 0;
    }
}

static void display() {
    int y, w, h;
    struct func_t *el;
    float percent_own, percent_total;
    size_t i;
    tb_clear();
    w = tb_width();
    h = tb_height();
    y = 0;
    tb_printf(0,  y++, TB_BOLD, 0, "%s", phpspy_args);
    tb_printf(0,  y++, 0, 0, "samp_count=%llu  err_count=%llu  func_count=%llu", samp_count, err_count, func_list_len);
    y++;
    tb_printf(0,  y,   TB_BOLD | TB_REVERSE, 0, "%-10s %-7s %-10s %-7s ", "NTOTAL", "TOTAL%", "NOWN", "OWN%");
    tb_printf(38, y++, TB_BOLD | TB_REVERSE, 0, "%-*s", w-38, "FUNC");
    i = 0;
    while (y < h && i < func_list_len) {
        el = func_list[i++];
        percent_own   = samp_count < 1 ? 0.f : (100.f * el->count_own   / samp_count);
        percent_total = samp_count < 1 ? 0.f : (100.f * el->count_total / samp_count);
        tb_printf(0, y++, 0, 0,
            "%-9llu  %-6.2f  %-9llu  %-6.2f  %s",
            el->count_total,
            percent_total,
            el->count_own,
            percent_own,
            el->func
        );
    }
    tb_present();
}


static void tb_print(const char *str, int x, int y, uint16_t fg, uint16_t bg) {
    uint32_t uni;
    while (*str) {
        str += tb_utf8_char_to_unicode(&uni, str);
        tb_change_cell(x, y, uni, fg, bg);
        x++;
    }
}

static void tb_printf(int x, int y, uint16_t fg, uint16_t bg, const char *fmt, ...) {
    char buf[4096];
    va_list vl;
    va_start(vl, fmt);
    vsnprintf(buf, sizeof(buf), fmt, vl);
    va_end(vl);
    tb_print(buf, x, y, fg, bg);
}

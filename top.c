#include "phpspy.h"

#define FUNC_SIZE 256
#define BUF_SIZE 512

typedef struct func_entry_s {
    char func[FUNC_SIZE];
    unsigned long count_excl;
    unsigned long count_incl;
    unsigned long total_count_excl;
    unsigned long total_count_incl;
    float percent_excl;
    UT_hash_handle hh;
} func_entry_t;

static int fork_child(int argc, char **argv, pid_t *pid, int *outfd, int *errfd);
static void filter_child_args(int argc, char **argv);
static void read_child_out(int fd);
static void read_child_err(int fd);
static void handle_line(char *line, int line_len);
static void handle_event(struct tb_event *event);
static void display();
static void tb_print(const char *str, int x, int y, uint16_t fg, uint16_t bg);
static void tb_printf(int x, int y, uint16_t fg, uint16_t bg, const char *fmt, ...);

static func_entry_t *func_map = NULL;
static func_entry_t **func_list = NULL;
static size_t func_list_len = 0;
static size_t func_list_size = 0;
static char buf[BUF_SIZE];
static size_t buf_len = 0;
static unsigned long total_samp_count = 0;
static unsigned long samp_count = 0;
static unsigned long total_err_count = 0;
static char phpspy_args[BUF_SIZE];

int main_top(int argc, char **argv) {
    fd_set readfds;
    int ttyfd, outfd, errfd, maxfd;
    pid_t pid;
    struct timeval timeout;
    struct timespec ts, last_display;
    struct tb_event event;
    int rc, i;
    /* TODO consider doing this by aggregating in-process instead of fork/stdout */

    outfd = errfd = ttyfd = pid = -1;

    if (opt_pid == -1 && opt_pgrep_args == NULL && optind >= argc) {
        /* TODO DRY with main() */
        fprintf(stderr, "Expected pid (-p), pgrep (-P), or command\n\n");
        usage(stderr, 1);
        return 1;
    }

    /* TODO refactor this function */
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

    tb_init(); /* TODO support for ncurses for portability sake? */
    while (!done) {
        clock_gettime(CLOCK_MONOTONIC, &ts);
        if (last_display.tv_sec == 0 || ts.tv_sec - last_display.tv_sec >= 1) {
            display();
            last_display = ts;
        }
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
        memmove(buf, nl+1, buf_len-(line_len+1));
        buf_len -= line_len+1;
    }
}

static void read_child_err(int fd) {
    /* TODO DRY with read_child_out */
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
        total_err_count += 1;
        buf_pos += nl-buf+1;
        read_rv -= nl-buf+1;
    }
}

static void handle_line(char *line, int line_len) {
    unsigned long frame_num;
    char *func;
    size_t func_len;
    func_entry_t *func_el;

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
        func_el = calloc(1, sizeof(func_entry_t));
        snprintf(func_el->func, FUNC_SIZE, "%.*s", (int)func_len, func);
        HASH_ADD_STR(func_map, func, func_el);
        func_list_len += 1;
        if (func_list_len > func_list_size) {
            func_list = realloc(func_list, sizeof(func_entry_t*) * (func_list_size + 1024));
            func_list_size += 1024;
        }
        func_list[func_list_len-1] = func_el;
    }

    if (frame_num == 0) {
        samp_count += 1;
        func_el->count_excl += 1;
    }
    func_el->count_incl += 1;
}

static void handle_event(struct tb_event *event) {
    if (event->type != TB_EVENT_KEY) {
        return;
    } else if (event->ch == 'q') {
        done = 1;
    } else if (event->ch == 'c') {
        /* TODO top mode handle_event */
    }
}

static int func_list_compare(const void *a, const void *b) {
    func_entry_t *fa, *fb;
    fa = *((func_entry_t**)a);
    fb = *((func_entry_t**)b);
    if (fb->count_excl == fa->count_excl) {
        if (fb->count_incl == fa->count_incl) {
            if (fb->total_count_excl == fa->total_count_excl) {
                if (fb->total_count_incl == fa->total_count_incl) {
                    return 0;
                }
                return fb->total_count_incl > fa->total_count_incl ? 1 : -1;
            }
            return fb->total_count_excl > fa->total_count_excl ? 1 : -1;
        }
        return fb->count_incl > fa->count_incl ? 1 : -1;
    }
    return fb->count_excl > fa->count_excl ? 1 : -1;
}

static void display() {
    int y, w, h;
    func_entry_t *el;
    size_t i;

    if (func_list_len > 0) {
        qsort(func_list, func_list_len, sizeof(func_entry_t*), func_list_compare);
        for (i = 0; i < func_list_len; i++) {
            func_list[i]->total_count_excl += func_list[i]->count_excl;
            func_list[i]->total_count_incl += func_list[i]->count_incl;
            func_list[i]->percent_excl = samp_count < 1 ? 0.f : (100.f * func_list[i]->count_excl / samp_count);
        }
    }
    total_samp_count += samp_count;

    tb_clear();
    w = tb_width();
    h = tb_height();
    y = 0;
    tb_printf(0,  y++, TB_BOLD, 0, "%s", phpspy_args);
    tb_printf(0,  y++, 0, 0, "samp_count=%llu  err_count=%llu  func_count=%llu", total_samp_count, total_err_count, func_list_len);
    y++;
    tb_printf(
        0, y, TB_BOLD | TB_REVERSE, 0,
        "%-10s %-10s %-10s %-10s %-7s ",
        "tincl", "texcl", "incl", "excl", "excl%"
    );
    tb_printf(52, y++, TB_BOLD | TB_REVERSE, 0, "%-*s", w-52, "func");
    i = 0;
    while (y < h && i < func_list_len) {
        el = func_list[i++];
        tb_printf(0, y++, 0, 0,
            "%-9llu  %-9llu  %-9llu  %-9llu  %-6.2f  %s",
            el->total_count_incl,
            el->total_count_excl,
            el->count_incl,
            el->count_excl,
            el->percent_excl,
            el->func
        );
    }

    for (i = 0; i < func_list_len; i++) {
        func_list[i]->count_excl = 0;
        func_list[i]->count_incl = 0;
    }
    samp_count = 0;

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

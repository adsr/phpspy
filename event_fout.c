#include "phpspy.h"

typedef struct event_handler_fout_udata_s {
    int fd;
    char *buf;
    char *cur;
    size_t buf_size;
    size_t rem;
    int use_mutex;
} event_handler_fout_udata_t;

static int event_handler_fout_write(event_handler_fout_udata_t *udata);
static int event_handler_fout_snprintf(char **s, size_t *n, size_t *ret_len, int repl_delim, const char *fmt, ...);
static int event_handler_fout_open(int *fd);
static phpspy_mutex_t event_handler_fout_mutex;

int event_handler_fout(struct trace_context_s *context, int event_type) {
    int rv, fd;
    size_t len;
    trace_frame_t *frame;
    trace_request_t *request;
    event_handler_fout_udata_t *udata;
    struct timeval tv;

    udata = (event_handler_fout_udata_t*)context->event_udata;
    if (!udata && event_type != PHPSPY_TRACE_EVENT_INIT) {
        return PHPSPY_ERR;
    }
    len = 0;
    switch (event_type) {
        case PHPSPY_TRACE_EVENT_INIT:
            try(rv, event_handler_fout_open(&fd));
            udata = calloc(1, sizeof(event_handler_fout_udata_t));
            udata->fd = fd;
            udata->buf_size = opt_fout_buffer_size + 1; /* + 1 for null char */
            udata->buf = malloc(udata->buf_size);
            udata->cur = udata->buf;
            udata->rem = udata->buf_size;
            udata->use_mutex = context->event_handler_opts != NULL
                && strchr(context->event_handler_opts, 'm') != NULL ? 1 : 0;
            context->event_udata = udata;
            phpspy_mutex_init(&event_handler_fout_mutex, NULL);
            break;
        case PHPSPY_TRACE_EVENT_STACK_BEGIN:
            udata->cur = udata->buf;
            udata->cur[0] = '\0';
            udata->rem = udata->buf_size;
            break;
        case PHPSPY_TRACE_EVENT_FRAME:
            frame = &context->event.frame;
            try(rv, event_handler_fout_snprintf(
                &udata->cur,
                &udata->rem,
                &len,
                1,
                "%d %.*s%s%.*s %.*s:%d",
                frame->depth,
                (int)frame->loc.class_len, frame->loc.class,
                frame->loc.class_len > 0 ? "::" : "",
                (int)frame->loc.func_len, frame->loc.func,
                (int)frame->loc.file_len, frame->loc.file,
                frame->loc.lineno
            ));
            try(rv, event_handler_fout_snprintf(&udata->cur, &udata->rem, &len, 0, "%c", opt_frame_delim));
            break;
        case PHPSPY_TRACE_EVENT_VARPEEK:
            try(rv, event_handler_fout_snprintf(
                &udata->cur,
                &udata->rem,
                &len,
                1,
                "# varpeek %s@%s = %s",
                context->event.varpeek.var->name,
                context->event.varpeek.entry->filename_lineno,
                context->event.varpeek.zval_str
            ));
            try(rv, event_handler_fout_snprintf(&udata->cur, &udata->rem, &len, 0, "%c", opt_frame_delim));
            break;
        case PHPSPY_TRACE_EVENT_GLOPEEK:
            try(rv, event_handler_fout_snprintf(
                &udata->cur,
                &udata->rem,
                &len,
                1,
                "# glopeek %s = %s",
                context->event.glopeek.gentry->key,
                context->event.glopeek.zval_str
            ));
            try(rv, event_handler_fout_snprintf(&udata->cur, &udata->rem, &len, 0, "%c", opt_frame_delim));
            break;
        case PHPSPY_TRACE_EVENT_REQUEST:
            request = &context->event.request;
            try(rv, event_handler_fout_snprintf(&udata->cur, &udata->rem, &len, 1, "# uri = %s", request->uri));
            try(rv, event_handler_fout_snprintf(&udata->cur, &udata->rem, &len, 0, "%c", opt_frame_delim));
            try(rv, event_handler_fout_snprintf(&udata->cur, &udata->rem, &len, 1, "# path = %s", request->path));
            try(rv, event_handler_fout_snprintf(&udata->cur, &udata->rem, &len, 0, "%c", opt_frame_delim));
            try(rv, event_handler_fout_snprintf(&udata->cur, &udata->rem, &len, 1, "# qstring = %s", request->qstring));
            try(rv, event_handler_fout_snprintf(&udata->cur, &udata->rem, &len, 0, "%c", opt_frame_delim));
            try(rv, event_handler_fout_snprintf(&udata->cur, &udata->rem, &len, 1, "# cookie = %s", request->cookie));
            try(rv, event_handler_fout_snprintf(&udata->cur, &udata->rem, &len, 0, "%c", opt_frame_delim));
            try(rv, event_handler_fout_snprintf(&udata->cur, &udata->rem, &len, 1, "# ts = %f", request->ts));
            try(rv, event_handler_fout_snprintf(&udata->cur, &udata->rem, &len, 0, "%c", opt_frame_delim));
            break;
        case PHPSPY_TRACE_EVENT_MEM:
            try(rv, event_handler_fout_snprintf(
                &udata->cur,
                &udata->rem,
                &len,
                1,
                "# mem %lu %lu",
                (uint64_t)context->event.mem.size,
                (uint64_t)context->event.mem.peak
            ));
            try(rv, event_handler_fout_snprintf(&udata->cur, &udata->rem, &len, 0, "%c", opt_frame_delim));
            break;
        case PHPSPY_TRACE_EVENT_STACK_END:
            if (udata->cur == udata->buf) {
                /* buffer is empty */
                break;
            }
            if (opt_filter_re) {
                rv = regexec(opt_filter_re, udata->buf, 0, NULL, 0);
                if (opt_filter_negate == 0 && rv != 0) break;
                if (opt_filter_negate != 0 && rv == 0) break;
            }
            do {
                if (opt_verbose_fields_ts) {
                    gettimeofday(&tv, NULL);
                    try_break(rv, event_handler_fout_snprintf(&udata->cur, &udata->rem, &len, 1, "# trace_ts = %f", (double)(tv.tv_sec + tv.tv_usec / 1000000.0)));
                    try_break(rv, event_handler_fout_snprintf(&udata->cur, &udata->rem, &len, 0, "%c", opt_frame_delim));
                }
                if (opt_verbose_fields_pid) {
                    try_break(rv, event_handler_fout_snprintf(&udata->cur, &udata->rem, &len, 1, "# pid = %d", context->target.pid));
                    try_break(rv, event_handler_fout_snprintf(&udata->cur, &udata->rem, &len, 0, "%c", opt_frame_delim));
                }
                try_break(rv, event_handler_fout_snprintf(&udata->cur, &udata->rem, &len, 0, "%c", opt_trace_delim));
            } while (0);
            try(rv, event_handler_fout_write(udata));
            break;
        case PHPSPY_TRACE_EVENT_DEINIT:
            close(udata->fd);
            free(udata->buf);
            free(udata);
            phpspy_mutex_destroy(&event_handler_fout_mutex);
            break;
    }
    return PHPSPY_OK;
}

static int event_handler_fout_write(event_handler_fout_udata_t *udata) {
    int rv;
    ssize_t write_len;

    rv = PHPSPY_OK;
    write_len = (udata->cur - udata->buf);

    if (write_len < 1) {
        /* nothing to write */
        return rv;
    }

    if (udata->use_mutex) {
        phpspy_mutex_lock(&event_handler_fout_mutex);
    }

    if (write(udata->fd, udata->buf, write_len) != write_len) {
        log_error("event_handler_fout: Write failed (%s)\n", errno != 0 ? strerror(errno) : "partial");
        rv = PHPSPY_ERR;
    }

    if (udata->use_mutex) {
        phpspy_mutex_unlock(&event_handler_fout_mutex);
    }

    return rv;
}

static int event_handler_fout_snprintf(char **s, size_t *n, size_t *ret_len, int repl_delim, const char *fmt, ...) {
    int len, i;
    va_list vl;
    char *c;

    va_start(vl, fmt);
    len = vsnprintf(*s, *n, fmt, vl);
    va_end(vl);

    if (len < 0 || (size_t)len >= *n) {
        log_error("event_handler_fout_snprintf: Not enough space in buffer; truncating\n");
        return PHPSPY_ERR | PHPSPY_ERR_BUF_FULL;
    }

    if (repl_delim) {
        for (i = 0; i < len; i++) { /* TODO optimize */
            c = *s + i;
            if (*c == opt_trace_delim || *c == opt_frame_delim) {
                *c = '?';
            }
        }
    }

    *s += len;
    *n -= len;
    *ret_len = (size_t)len;

    return PHPSPY_OK;
}

static int event_handler_fout_open(int *fd) {
    int tfd = -1, errno_saved;
    char *path, *apath = NULL;

    if (strcmp(opt_path_output, "-") == 0) {
        tfd = dup(STDOUT_FILENO);
        if (tfd < 0) {
            perror("event_handler_fout_open: dup");
            return PHPSPY_ERR;
        }
        *fd = tfd;
        return PHPSPY_OK;
    }

    if (strstr(opt_path_output, "%d") != NULL) {
        if (asprintf(&apath, opt_path_output, gettid()) < 0) {
            errno = ENOMEM;
            perror("event_handler_fout_open: asprintf");
            return PHPSPY_ERR;
        }
        path = apath;
    } else {
        path = opt_path_output;
    }
#ifdef PHPSPY_WIN32
    tfd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
#else
    tfd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
#endif
    errno_saved = errno;

    if (apath) {
        free(apath);
    }

    if (tfd < 0) {
        errno = errno_saved;
        perror("event_handler_fout_open: open");
        return PHPSPY_ERR;
    }

    *fd = tfd;
    return PHPSPY_OK;
}

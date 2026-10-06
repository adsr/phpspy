#include "phpspy.h"

typedef struct event_handler_fout_udata_s {
    int fd;
    char *buf;
    size_t len;
    size_t cap;
    int use_mutex;
    int truncated;
    char trunc_marker[32];
    char trace_id_tail[64];
    size_t trunc_marker_len;
    size_t trace_id_tail_len;
} event_handler_fout_udata_t;

static int event_handler_fout_write(event_handler_fout_udata_t *udata);
static int event_handler_fout_write_whole(event_handler_fout_udata_t *udata);
static int event_handler_fout_write_chunked(event_handler_fout_udata_t *udata);
static int event_handler_fout_write_chunk(event_handler_fout_udata_t *udata, char *body, size_t body_len, char sentinel);
static char *event_handler_fout_find_chunk_end(char *chunk_start, char *stop, size_t max_chunk_len);
static void event_handler_fout_fill_trace_id(event_handler_fout_udata_t *udata, uint64_t trace_id, uint64_t chunk_idx);
static void event_handler_fout_iov_set(struct iovec *iov, void *base, size_t len);
static int event_handler_fout_writev(int fd, struct iovec *iov, int iovcnt);
static void event_handler_fout_snprintf(event_handler_fout_udata_t *udata, int strip_delim, const char *fmt, ...);
static void event_handler_fout_truncate(event_handler_fout_udata_t *udata);
static int event_handler_fout_realloc(event_handler_fout_udata_t *udata, size_t needed_cap);
static int event_handler_fout_open(int *fd);
static pthread_mutex_t event_handler_fout_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint64_t event_handler_fout_trace_id = 0;
static int event_handler_fout_shared_fd = -1;

int event_handler_fout(struct trace_context_s *context, int event_type) {
    int rv, fd;
    trace_frame *frame;
    trace_request *request;
    event_handler_fout_udata_t *udata;
    struct timeval tv;

    udata = (event_handler_fout_udata_t*)context->event_udata;
    if (!udata && event_type != PHPSPY_TRACE_EVENT_INIT) {
        return PHPSPY_ERR;
    }

    switch (event_type) {
        case PHPSPY_TRACE_EVENT_INIT:
            try(rv, event_handler_fout_open(&fd));
            udata = calloc(1, sizeof(event_handler_fout_udata_t));
            udata->fd = fd;
            udata->cap = opt_fout_buffer_size;
            try(rv, event_handler_fout_realloc(udata, udata->cap));
            udata->use_mutex = context->event_handler_opts != NULL
                && strchr(context->event_handler_opts, 'm') != NULL ? 1 : 0;
            udata->trunc_marker_len = (size_t)snprintf(
                udata->trunc_marker,
                sizeof(udata->trunc_marker),
                "# truncated = 1%c",
                opt_frame_delim
            );
            context->event_udata = udata;
            break;
        case PHPSPY_TRACE_EVENT_STACK_BEGIN:
            udata->len = 0;
            udata->buf[0] = '\0';
            udata->truncated = 0;
            break;
        case PHPSPY_TRACE_EVENT_FRAME:
            frame = &context->event.frame;
            event_handler_fout_snprintf(
                udata,
                1,
                "%d %.*s%s%.*s %.*s:%d",
                frame->depth,
                (int)frame->loc.class_len, frame->loc.class,
                frame->loc.class_len > 0 ? "::" : "",
                (int)frame->loc.func_len, frame->loc.func,
                (int)frame->loc.file_len, frame->loc.file,
                frame->loc.lineno
            );
            event_handler_fout_snprintf(udata, 0, "%c", opt_frame_delim);
            break;
        case PHPSPY_TRACE_EVENT_VARPEEK:
            event_handler_fout_snprintf(
                udata,
                1,
                "# varpeek %s@%s = %.*s",
                context->event.varpeek.var->name,
                context->event.varpeek.entry->filename_lineno,
                context->event.varpeek.zval_str_len,
                context->event.varpeek.zval_str
            );
            event_handler_fout_snprintf(udata, 0, "%c", opt_frame_delim);
            break;
        case PHPSPY_TRACE_EVENT_GLOPEEK:
            event_handler_fout_snprintf(
                udata,
                1,
                "# glopeek %s = %.*s",
                context->event.glopeek.gentry->key,
                context->event.glopeek.zval_str_len,
                context->event.glopeek.zval_str
            );
            event_handler_fout_snprintf(udata, 0, "%c", opt_frame_delim);
            break;
        case PHPSPY_TRACE_EVENT_REQUEST:
            request = &context->event.request;
            event_handler_fout_snprintf(udata, 1, "# uri = %s", request->uri);
            event_handler_fout_snprintf(udata, 0, "%c", opt_frame_delim);
            event_handler_fout_snprintf(udata, 1, "# path = %s", request->path);
            event_handler_fout_snprintf(udata, 0, "%c", opt_frame_delim);
            event_handler_fout_snprintf(udata, 1, "# qstring = %s", request->qstring);
            event_handler_fout_snprintf(udata, 0, "%c", opt_frame_delim);
            event_handler_fout_snprintf(udata, 1, "# cookie = %s", request->cookie);
            event_handler_fout_snprintf(udata, 0, "%c", opt_frame_delim);
            event_handler_fout_snprintf(udata, 1, "# ts = %f", request->ts);
            event_handler_fout_snprintf(udata, 0, "%c", opt_frame_delim);
            break;
        case PHPSPY_TRACE_EVENT_MEM:
            event_handler_fout_snprintf(
                udata,
                1,
                "# mem %lu %lu",
                (uint64_t)context->event.mem.size,
                (uint64_t)context->event.mem.peak
            );
            event_handler_fout_snprintf(udata, 0, "%c", opt_frame_delim);
            break;
        case PHPSPY_TRACE_EVENT_STACK_END:
            if (udata->len == 0) break;
            if (opt_filter_re) {
                rv = regexec(opt_filter_re, udata->buf, 0, NULL, 0);
                if (opt_filter_negate == 0 && rv != 0) return PHPSPY_ERR_SKIPPED;
                if (opt_filter_negate != 0 && rv == 0) return PHPSPY_ERR_SKIPPED;
            }
            do {
                if (opt_verbose_fields_ts) {
                    gettimeofday(&tv, NULL);
                    event_handler_fout_snprintf(udata, 1, "# trace_ts = %f", (double)(tv.tv_sec + tv.tv_usec / 1000000.0));
                    event_handler_fout_snprintf(udata, 0, "%c", opt_frame_delim);
                }
                if (opt_verbose_fields_pid) {
                    event_handler_fout_snprintf(udata, 1, "# pid = %d", context->target.pid);
                    event_handler_fout_snprintf(udata, 0, "%c", opt_frame_delim);
                }
            } while (0);
            try(rv, event_handler_fout_write(udata));
            break;
        case PHPSPY_TRACE_EVENT_DEINIT:
            if (udata->fd != event_handler_fout_shared_fd) {
                close(udata->fd);
            }
            free(udata->buf);
            free(udata);
            break;
    }
    return PHPSPY_OK;
}

static int event_handler_fout_write(event_handler_fout_udata_t *udata) {
    int rv;
    size_t total_len;

    total_len = udata->len + 1; /* +1 for trace_delim */
    if (udata->truncated) total_len += udata->trunc_marker_len;

    if (udata->use_mutex) {
        pthread_mutex_lock(&event_handler_fout_mutex);
    }

    if (total_len <= (size_t)opt_fout_buffer_size) {
        rv = event_handler_fout_write_whole(udata);
    } else {
        rv = event_handler_fout_write_chunked(udata);
    }

    if (udata->use_mutex) {
        pthread_mutex_unlock(&event_handler_fout_mutex);
    }

    if (udata->truncated) rv |= PHPSPY_ERR_TRUNCATED;

    return rv;
}

static int event_handler_fout_write_whole(event_handler_fout_udata_t *udata) {
    struct iovec iov[3];
    int iovcnt;

    iovcnt = 0;
    event_handler_fout_iov_set(&iov[iovcnt++], udata->buf, udata->len);
    if (udata->truncated) {
        event_handler_fout_iov_set(&iov[iovcnt++], udata->trunc_marker, udata->trunc_marker_len);
    }
    event_handler_fout_iov_set(&iov[iovcnt++], &opt_trace_delim, 1);

    return event_handler_fout_writev(udata->fd, iov, iovcnt);
}

static int event_handler_fout_write_chunked(event_handler_fout_udata_t *udata) {
    uint64_t trace_id, chunk_idx;
    char *chunk_start, *chunk_end, *stop;
    size_t max_chunk_len;
    char sentinel;
    int rv;

    rv = PHPSPY_OK;
    trace_id = __atomic_fetch_add(&event_handler_fout_trace_id, 1, __ATOMIC_RELAXED);
    chunk_idx = 0;
    chunk_start = udata->buf;
    stop = udata->buf + udata->len;

    while (chunk_start < stop) {
        /* calc max_chunk_len */
        event_handler_fout_fill_trace_id(udata, trace_id, chunk_idx);
        max_chunk_len = (size_t)opt_fout_buffer_size > udata->trace_id_tail_len
            ? (size_t)opt_fout_buffer_size - udata->trace_id_tail_len
            : 0;

        /* find chunk_end */
        chunk_end = event_handler_fout_find_chunk_end(chunk_start, stop, max_chunk_len);

        /* break if single record exceeds max_chunk_len */
        if (!chunk_end) break;

        /* set sentinel according to whether this is the last chunk */
        sentinel = chunk_end == stop && !udata->truncated ? '!' : '~';

        rv |= event_handler_fout_write_chunk(udata, chunk_start, (size_t)(chunk_end - chunk_start), sentinel);

        chunk_start = chunk_end;
        chunk_idx += 1;
    }

    if (chunk_start < stop) udata->truncated = 1;

    if (!udata->truncated) return rv;

    /* write truncated metadata record */
    event_handler_fout_fill_trace_id(udata, trace_id, chunk_idx);
    rv |= event_handler_fout_write_chunk(udata, udata->trunc_marker, udata->trunc_marker_len, '!');

    return rv;
}

static int event_handler_fout_write_chunk(event_handler_fout_udata_t *udata, char *body, size_t body_len, char sentinel) {
    struct iovec iov[2];

    event_handler_fout_iov_set(&iov[0], body, body_len);

    /* set sentinel in trace_id_tail. see event_handler_fout_fill_trace_id
       for format. trace_id_tail_len should always be >= 3 */
    assert(udata->trace_id_tail_len >= 3);
    udata->trace_id_tail[udata->trace_id_tail_len - 3] = sentinel;
    event_handler_fout_iov_set(&iov[1], udata->trace_id_tail, udata->trace_id_tail_len);

    return event_handler_fout_writev(udata->fd, iov, 2);
}

static char *event_handler_fout_find_chunk_end(char *chunk_start, char *stop, size_t max_chunk_len) {
    char *last_delim;

    if ((size_t)(stop - chunk_start) <= max_chunk_len) {
        return stop;
    }

    last_delim = memrchr(chunk_start, opt_frame_delim, max_chunk_len);
    if (!last_delim) {
        return NULL;
    }

    return last_delim + 1;
}

static void event_handler_fout_fill_trace_id(event_handler_fout_udata_t *udata, uint64_t trace_id, uint64_t chunk_idx) {
    udata->trace_id_tail_len = (size_t)snprintf(
        udata->trace_id_tail,
        sizeof(udata->trace_id_tail),
        "# trace_id = %" PRIu64 ".%" PRIu64 "~%c%c",
        trace_id,
        chunk_idx,
        opt_frame_delim,
        opt_trace_delim
    );
}

static void event_handler_fout_iov_set(struct iovec *iov, void *base, size_t len) {
    iov->iov_base = base;
    iov->iov_len = len;
}

static int event_handler_fout_writev(int fd, struct iovec *iov, int iovcnt) {
    size_t total_len;
    int i;

    total_len = 0;
    for (i = 0; i < iovcnt; i++) {
        total_len += iov[i].iov_len;
    }

    if (writev(fd, iov, iovcnt) != (ssize_t)total_len) {
        log_error("event_handler_fout_writev: writev (%s)\n", errno != 0 ? strerror(errno) : "partial");
        return PHPSPY_ERR;
    }

    return PHPSPY_OK;
}

static void event_handler_fout_snprintf(event_handler_fout_udata_t *udata, int strip_delim, const char *fmt, ...) {
    int append_len, i;
    va_list vl;
    char *delim, *cur, *stop;
    size_t rem_cap;
    char delims[2] = { opt_frame_delim, opt_trace_delim };

    if (udata->truncated) return;

    va_start(vl, fmt);
    rem_cap = udata->cap - udata->len;
    append_len = vsnprintf(udata->buf + udata->len, rem_cap, fmt, vl);
    va_end(vl);

    if (append_len < 0) {
        log_perror("event_handler_fout_snprintf: vsnprintf");
        event_handler_fout_truncate(udata);
        return;
    }

    if ((size_t)append_len >= rem_cap) {
        if (event_handler_fout_realloc(udata, udata->len + (size_t)append_len + 1) != PHPSPY_OK) {
            event_handler_fout_truncate(udata);
            return;
        }

        va_start(vl, fmt);
        rem_cap = udata->cap - udata->len;
        append_len = vsnprintf(udata->buf + udata->len, rem_cap, fmt, vl);
        va_end(vl);
    }

    if (strip_delim) {
        cur = udata->buf + udata->len;
        stop = cur + append_len;
        for (i = 0; i < 2; i++) {
            while (cur < stop) {
                delim = memchr(cur, delims[i], (size_t)(stop - cur));
                if (!delim) break;
                *delim = '?';
                cur = delim + 1;
            }
        }
    }

    udata->len += (size_t)append_len;
}

static void event_handler_fout_truncate(event_handler_fout_udata_t *udata) {
    char *last_delim;

    udata->truncated = 1;

    last_delim = memrchr(udata->buf, opt_frame_delim, udata->len);
    udata->len = last_delim ? (size_t)(last_delim + 1 - udata->buf) : 0;
    udata->buf[udata->len] = '\0';
}

static int event_handler_fout_realloc(event_handler_fout_udata_t *udata, size_t needed_cap) {
    size_t new_cap;
    char *new_buf;

    if (needed_cap > PHPSPY_FOUT_MAX_BUFFER) {
        return PHPSPY_ERR;
    }

    new_cap = udata->cap;
    while (new_cap < needed_cap) {
        new_cap *= 2;
    }
    if (new_cap > PHPSPY_FOUT_MAX_BUFFER) {
        new_cap = PHPSPY_FOUT_MAX_BUFFER;
    }

    new_buf = realloc(udata->buf, new_cap);
    if (!new_buf) {
        log_perror("event_handler_fout_realloc: realloc");
        return PHPSPY_ERR;
    }

    udata->buf = new_buf;
    udata->cap = new_cap;

    return PHPSPY_OK;
}

static int event_handler_fout_open(int *fd) {
    int tfd = -1, errno_saved, rv, flags, mode;
    char *apath = NULL;

    rv = PHPSPY_OK;
    flags = O_WRONLY | O_CREAT | O_TRUNC;
    mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

    pthread_mutex_lock(&event_handler_fout_mutex);

    if (strstr(opt_path_output, "%d") != NULL) {
        if (asprintf(&apath, opt_path_output, gettid()) < 0) {
            errno = ENOMEM;
            log_perror("event_handler_fout_open: asprintf");
            rv = PHPSPY_ERR;
            goto event_handler_fout_open_end;
        }
        tfd = open(apath, flags, mode);
        errno_saved = errno;
        free(apath);
    } else {
        if (strcmp(opt_path_output, "-") == 0) {
            tfd = dup(STDOUT_FILENO);
        } else if (event_handler_fout_shared_fd < 0) {
            tfd = open(opt_path_output, flags, mode);
            event_handler_fout_shared_fd = tfd;
        } else {
            tfd = dup(event_handler_fout_shared_fd);
        }
        errno_saved = errno;
    }

    if (tfd < 0) {
        errno = errno_saved;
        log_perror("event_handler_fout_open: open");
        rv = PHPSPY_ERR;
        goto event_handler_fout_open_end;
    }

    *fd = tfd;

event_handler_fout_open_end:
    pthread_mutex_unlock(&event_handler_fout_mutex);
    return rv;
}

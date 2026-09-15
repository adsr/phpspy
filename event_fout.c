#include "phpspy.h"

typedef struct event_handler_fout_udata_s {
    int fd;
    char *buf;       /* assembled trace; always NUL-terminated at buf[used] */
    size_t used;     /* payload length, not counting the NUL */
    size_t alloc;    /* bytes allocated; always >= used + 1 */
    int single_line; /* -1 mode; chunking is disabled */
    int trunc;       /* this trace was truncated; reset at STACK_BEGIN */
    int warned;      /* truncation already logged by this handler */
    int use_mutex;
} event_handler_fout_udata_t;

static int event_handler_fout_flush(event_handler_fout_udata_t *udata);
static size_t event_handler_fout_chunk_end(const char *buf, size_t used, size_t start, size_t cap, int *oversize);
static int event_handler_fout_writev_all(int fd, struct iovec *iov, int iovcnt);
static int event_handler_fout_reserve(event_handler_fout_udata_t *udata, size_t need, size_t limit);
static void event_handler_fout_truncated(event_handler_fout_udata_t *udata);
static void event_handler_fout_vrecord(event_handler_fout_udata_t *udata, size_t limit, int honor_trunc, const char *fmt, va_list vl);
static void event_handler_fout_record(event_handler_fout_udata_t *udata, const char *fmt, ...);
static void event_handler_fout_record_epi(event_handler_fout_udata_t *udata, const char *fmt, ...);
static int event_handler_fout_open(int *fd);
static pthread_mutex_t event_handler_fout_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint64_t event_handler_fout_trace_id = 0;

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
            udata->alloc = (size_t)opt_fout_buffer_size + 1; /* + 1 for null char */
            udata->buf = malloc(udata->alloc);
            if (!udata->buf) {
                log_error("event_handler_fout: Failed to allocate %lu bytes\n", (unsigned long)udata->alloc);
                close(fd);
                free(udata);
                return PHPSPY_ERR;
            }
            udata->used = 0;
            udata->buf[0] = '\0';
            udata->single_line = opt_trace_delim != opt_frame_delim ? 1 : 0;
            udata->use_mutex = context->event_handler_opts != NULL
                && strchr(context->event_handler_opts, 'm') != NULL ? 1 : 0;
            context->event_udata = udata;
            break;
        case PHPSPY_TRACE_EVENT_STACK_BEGIN:
            /* keep alloc; a per-thread high water mark avoids realloc churn */
            udata->used = 0;
            udata->buf[0] = '\0';
            udata->trunc = 0;
            break;
        case PHPSPY_TRACE_EVENT_FRAME:
            frame = &context->event.frame;
            event_handler_fout_record(
                udata,
                "%d %.*s%s%.*s %.*s:%d",
                frame->depth,
                (int)frame->loc.class_len, frame->loc.class,
                frame->loc.class_len > 0 ? "::" : "",
                (int)frame->loc.func_len, frame->loc.func,
                (int)frame->loc.file_len, frame->loc.file,
                frame->loc.lineno
            );
            break;
        case PHPSPY_TRACE_EVENT_VARPEEK:
            event_handler_fout_record(
                udata,
                "# varpeek %s@%s = %.*s",
                context->event.varpeek.var->name,
                context->event.varpeek.entry->filename_lineno,
                (int)context->event.varpeek.zval_str_len,
                context->event.varpeek.zval_str
            );
            break;
        case PHPSPY_TRACE_EVENT_GLOPEEK:
            event_handler_fout_record(
                udata,
                "# glopeek %s = %.*s",
                context->event.glopeek.gentry->key,
                (int)context->event.glopeek.zval_str_len,
                context->event.glopeek.zval_str
            );
            break;
        case PHPSPY_TRACE_EVENT_REQUEST:
            request = &context->event.request;
            event_handler_fout_record(udata, "# uri = %s", request->uri);
            event_handler_fout_record(udata, "# path = %s", request->path);
            event_handler_fout_record(udata, "# qstring = %s", request->qstring);
            event_handler_fout_record(udata, "# cookie = %s", request->cookie);
            event_handler_fout_record(udata, "# ts = %f", request->ts);
            break;
        case PHPSPY_TRACE_EVENT_MEM:
            event_handler_fout_record(
                udata,
                "# mem %lu %lu",
                (uint64_t)context->event.mem.size,
                (uint64_t)context->event.mem.peak
            );
            break;
        case PHPSPY_TRACE_EVENT_STACK_END:
            if (udata->used == 0) {
                /* buffer is empty */
                break;
            }
            if (opt_filter_re) {
                rv = regexec(opt_filter_re, udata->buf, 0, NULL, 0);
                if (opt_filter_negate == 0 && rv != 0) return PHPSPY_ERR_SKIPPED;
                if (opt_filter_negate != 0 && rv == 0) return PHPSPY_ERR_SKIPPED;
            }
            if (opt_verbose_fields_ts) {
                gettimeofday(&tv, NULL);
                event_handler_fout_record_epi(udata, "# trace_ts = %f", (double)(tv.tv_sec + tv.tv_usec / 1000000.0));
            }
            if (opt_verbose_fields_pid) {
                event_handler_fout_record_epi(udata, "# pid = %d", context->target.pid);
            }
            try(rv, event_handler_fout_flush(udata));
            /* the trace was written, so a truncated sample still counts toward
               `--limit` via PHPSPY_TRACE_COUNTED. A failed write returns plain
               PHPSPY_ERR above and must not count. */
            return udata->trunc ? PHPSPY_ERR_BUF_FULL : PHPSPY_OK;
        case PHPSPY_TRACE_EVENT_DEINIT:
            close(udata->fd);
            free(udata->buf);
            free(udata);
            break;
    }
    return PHPSPY_OK;
}

/**
 * Split the assembled trace into chunks of at most `opt_fout_buffer_size`
 * bytes and write each with a single writev(2).
 */
static int event_handler_fout_flush(event_handler_fout_udata_t *udata) {
    int rv, oversize, mlen;
    unsigned int k, nchunks;
    size_t cap, start, end, used;
    uint64_t id;
    char marker[PHPSPY_FOUT_CHUNK_RESERVE];
    struct iovec iov[2];

    cap = (size_t)opt_fout_buffer_size - (size_t)PHPSPY_FOUT_CHUNK_RESERVE;
    used = udata->used;
    start = 0;
    nchunks = 0;

    /* pass 1: count chunks and settle truncation before the ids are taken */
    while (start < used) {
        end = event_handler_fout_chunk_end(udata->buf, used, start, cap, &oversize);
        if (oversize) {
            /* one record is longer than a chunk; cut it, keep it delimited */
            udata->buf[end - 1] = opt_frame_delim;
            event_handler_fout_truncated(udata);
            nchunks += 1;
            used = end;
            break;
        }
        nchunks += 1;
        start = end;
        if (udata->single_line && start < used) {
            /* a trace must stay on one line, so it cannot be chunked */
            event_handler_fout_truncated(udata);
            used = start;
            break;
        }
    }
    udata->buf[used] = '\0';
    udata->used = used;
    if (udata->trunc) {
        event_handler_fout_record_epi(udata, "# truncated = 1");
        used = udata->used;
    }

    /* pass 2: write. The id is taken outside the lock, so ids may reach the
       stream slightly out of order; the reader keys on them, not on order. */
    id = __atomic_fetch_add(&event_handler_fout_trace_id, 1, __ATOMIC_RELAXED);
    rv = PHPSPY_OK;
    if (udata->use_mutex) {
        pthread_mutex_lock(&event_handler_fout_mutex);
    }
    start = 0;
    for (k = 0; k < nchunks; k++) {
        /* the last chunk also carries the record appended after pass 1 */
        end = k + 1 == nchunks
            ? used
            : event_handler_fout_chunk_end(udata->buf, used, start, cap, &oversize);
        mlen = snprintf(marker, sizeof(marker), "# trace_id = %llu.%u/%u%c",
            (unsigned long long)id, k, nchunks, opt_frame_delim);
        if (mlen < 0 || (size_t)mlen + 1 >= sizeof(marker)) {
            log_error("event_handler_fout: Failed to format trace_id marker\n");
            rv = PHPSPY_ERR;
            break;
        }
        if (k + 1 == nchunks) {
            marker[mlen++] = opt_trace_delim;
        }
        iov[0].iov_base = udata->buf + start;
        iov[0].iov_len = end - start;
        iov[1].iov_base = marker;
        iov[1].iov_len = (size_t)mlen;
        if (event_handler_fout_writev_all(udata->fd, iov, 2) != PHPSPY_OK) {
            rv = PHPSPY_ERR;
            break;
        }
        start = end;
    }
    if (udata->use_mutex) {
        pthread_mutex_unlock(&event_handler_fout_mutex);
    }

    return rv;
}

/**
 * Return the end offset of the chunk starting at `start`. The buffer always
 * ends with `opt_frame_delim`, so scanning back from the cap lands on a record
 * boundary unless a single record is longer than `cap`.
 */
static size_t event_handler_fout_chunk_end(const char *buf, size_t used, size_t start, size_t cap, int *oversize) {
    size_t end, i;

    *oversize = 0;
    end = start + cap < used ? start + cap : used;
    if (end == used) {
        return end;
    }
    for (i = end; i > start; i--) {
        if (buf[i - 1] == opt_frame_delim) {
            return i;
        }
    }
    *oversize = 1;
    return start + cap;
}

static int event_handler_fout_writev_all(int fd, struct iovec *iov, int iovcnt) {
    ssize_t n;

    while (iovcnt > 0) {
        n = writev(fd, iov, iovcnt);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            log_error("event_handler_fout: Write failed (%s)\n", strerror(errno));
            return PHPSPY_ERR;
        }
        while (iovcnt > 0 && (size_t)n >= iov->iov_len) {
            n -= (ssize_t)iov->iov_len;
            iov += 1;
            iovcnt -= 1;
        }
        if (iovcnt > 0) {
            iov->iov_base = (char *)iov->iov_base + n;
            iov->iov_len -= (size_t)n;
        }
    }

    return PHPSPY_OK;
}

/**
 * Ensure `need` bytes are available past `used`, growing the buffer up to a
 * payload of `limit` bytes.
 */
static int event_handler_fout_reserve(event_handler_fout_udata_t *udata, size_t need, size_t limit) {
    size_t alloc;
    char *buf;

    if (udata->used + need <= udata->alloc) {
        return PHPSPY_OK;
    }
    if (udata->used + need > limit + 1) {
        return PHPSPY_ERR_BUF_FULL;
    }
    alloc = udata->alloc;
    while (alloc < udata->used + need) {
        /* clamp before doubling so that `alloc` cannot overflow */
        if (alloc > (limit + 1) / 2) {
            alloc = limit + 1;
            break;
        }
        alloc *= 2;
    }
    buf = realloc(udata->buf, alloc);
    if (!buf) {
        log_error("event_handler_fout: Failed to allocate %lu bytes\n", (unsigned long)alloc);
        return PHPSPY_ERR;
    }
    udata->buf = buf;
    udata->alloc = alloc;

    return PHPSPY_OK;
}

static void event_handler_fout_truncated(event_handler_fout_udata_t *udata) {
    udata->trunc = 1;
    if (!udata->warned) {
        udata->warned = 1;
        log_error(
            "event_handler_fout: trace truncated (record exceeds -b payload, "
            "single-line mode, or %u-byte trace cap); further truncations not reported\n",
            PHPSPY_FOUT_MAX_TRACE
        );
    }
}

/**
 * Append one record plus `opt_frame_delim` to the buffer. Body and delimiter
 * are appended together so that the buffer always ends with a delimiter, which
 * is what lets the chunker find record boundaries by scanning back.
 */
static void event_handler_fout_vrecord(event_handler_fout_udata_t *udata, size_t limit, int honor_trunc, const char *fmt, va_list vl) {
    int len, i;
    char *c;
    va_list vl2;

    if (honor_trunc && udata->trunc) {
        /* never emit a later record after refusing an earlier one */
        return;
    }

    va_copy(vl2, vl);
    len = vsnprintf(udata->buf + udata->used, udata->alloc - udata->used, fmt, vl);
    if (len < 0) {
        udata->buf[udata->used] = '\0';
        event_handler_fout_truncated(udata);
        va_end(vl2);
        return;
    }
    if ((size_t)len + 2 > udata->alloc - udata->used) {
        /* + 2 for the delimiter and the null char */
        if (event_handler_fout_reserve(udata, (size_t)len + 2, limit) != PHPSPY_OK) {
            udata->buf[udata->used] = '\0';
            event_handler_fout_truncated(udata);
            va_end(vl2);
            return;
        }
        len = vsnprintf(udata->buf + udata->used, udata->alloc - udata->used, fmt, vl2);
        if (len < 0 || (size_t)len + 2 > udata->alloc - udata->used) {
            udata->buf[udata->used] = '\0';
            event_handler_fout_truncated(udata);
            va_end(vl2);
            return;
        }
    }
    va_end(vl2);

    for (i = 0; i < len; i++) { /* TODO optimize */
        c = udata->buf + udata->used + i;
        if (*c == opt_trace_delim || *c == opt_frame_delim) {
            *c = '?';
        }
    }

    udata->buf[udata->used + (size_t)len] = opt_frame_delim;
    udata->used += (size_t)len + 1;
    udata->buf[udata->used] = '\0';
}

static void event_handler_fout_record(event_handler_fout_udata_t *udata, const char *fmt, ...) {
    va_list vl;

    va_start(vl, fmt);
    event_handler_fout_vrecord(udata, PHPSPY_FOUT_MAX_TRACE - PHPSPY_FOUT_EPILOGUE_RESERVE, 1, fmt, vl);
    va_end(vl);
}

static void event_handler_fout_record_epi(event_handler_fout_udata_t *udata, const char *fmt, ...) {
    va_list vl;

    va_start(vl, fmt);
    event_handler_fout_vrecord(udata, PHPSPY_FOUT_MAX_TRACE, 0, fmt, vl);
    va_end(vl);
}

static int event_handler_fout_open(int *fd) {
    int tfd = -1, errno_saved;
    char *path, *apath = NULL;

    if (strcmp(opt_path_output, "-") == 0) {
        tfd = dup(STDOUT_FILENO);
        if (tfd < 0) {
            log_perror("event_handler_fout_open: dup");
            return PHPSPY_ERR;
        }
        *fd = tfd;
        return PHPSPY_OK;
    }

    if (strstr(opt_path_output, "%d") != NULL) {
        if (asprintf(&apath, opt_path_output, gettid()) < 0) {
            errno = ENOMEM;
            log_perror("event_handler_fout_open: asprintf");
            return PHPSPY_ERR;
        }
        path = apath;
    } else {
        path = opt_path_output;
    }

    tfd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    errno_saved = errno;

    if (apath) {
        free(apath);
    }

    if (tfd < 0) {
        errno = errno_saved;
        log_perror("event_handler_fout_open: open");
        return PHPSPY_ERR;
    }

    *fd = tfd;
    return PHPSPY_OK;
}

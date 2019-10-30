#include "phpspy.h"

typedef struct event_handler_fout_udata_s {
    FILE *fout;
    int fdout;
    char buf[4097]; /* writes lte PIPE_BUF (4kb) are atomic (+ 1 for null char) */
    char *cur;
    size_t rem;
} event_handler_fout_udata_t;

static int event_handler_fout_open(FILE **fout);

static int event_handler_fout_snprintf(char **s, size_t *n, size_t *ret_len, const char *fmt, ...);

int event_handler_fout(struct trace_context_s *context, int event_type) {
    int rv;
    FILE *fout;
    size_t len;
    ssize_t write_len;
    trace_frame_t *frame;
    trace_request_t *request;
    event_handler_fout_udata_t *udata;

    udata = (event_handler_fout_udata_t*)context->event_udata;
    if (!udata && event_type != PHPSPY_TRACE_EVENT_INIT) {
        return 1;
    }
    len = 0;
    switch (event_type) {
        case PHPSPY_TRACE_EVENT_INIT:
            try(rv, event_handler_fout_open(&fout));
            udata = calloc(1, sizeof(event_handler_fout_udata_t));
            udata->fout = fout;
            udata->fdout = fileno(fout);
            udata->cur = udata->buf;
            udata->rem = sizeof(udata->buf);
            context->event_udata = udata;
            break;
        case PHPSPY_TRACE_EVENT_STACK_BEGIN:
            udata->cur = udata->buf;
            udata->rem = sizeof(udata->buf);
            break;
        case PHPSPY_TRACE_EVENT_FRAME:
            frame = &context->event.frame;
            try(rv, event_handler_fout_snprintf(
                &udata->cur,
                &udata->rem,
                &len,
                "%d %.*s%s%.*s %.*s:%d%s",
                frame->depth,
                (int)frame->loc.class_len, frame->loc.class,
                frame->loc.class_len > 0 ? "::" : "",
                (int)frame->loc.func_len, frame->loc.func,
                (int)frame->loc.file_len, frame->loc.file,
                frame->loc.lineno,
                opt_frame_delim
            ));
            break;
        case PHPSPY_TRACE_EVENT_VARPEEK:
            try(rv, event_handler_fout_snprintf(
                &udata->cur,
                &udata->rem,
                &len,
                "# varpeek %s@%s = %s%s",
                context->event.varpeek.var->name,
                context->event.varpeek.entry->filename_lineno,
                context->event.varpeek.zval_str,
                opt_frame_delim
            ));
            break;
        case PHPSPY_TRACE_EVENT_GLOPEEK:
            try(rv, event_handler_fout_snprintf(
                &udata->cur,
                &udata->rem,
                &len,
                "# glopeek %s = %s%s",
                context->event.glopeek.gentry->key,
                context->event.glopeek.zval_str,
                opt_frame_delim
            ));
            break;
        case PHPSPY_TRACE_EVENT_REQUEST:
            request = &context->event.request;
            try(rv, event_handler_fout_snprintf(
                &udata->cur,
                &udata->rem,
                &len,
                "# uri = %s%s"
                "# path = %s%s"
                "# qstring = %s%s"
                "# cookie = %s%s"
                "# ts = %f%s",
                request->uri, opt_frame_delim,
                request->path, opt_frame_delim,
                request->qstring, opt_frame_delim,
                request->cookie, opt_frame_delim,
                request->ts, opt_frame_delim
            ));
            break;
        case PHPSPY_TRACE_EVENT_MEM:
            try(rv, event_handler_fout_snprintf(
                &udata->cur,
                &udata->rem,
                &len,
                "# mem %lu %lu%s",
                (uint64_t)context->event.mem.size,
                (uint64_t)context->event.mem.peak,
                opt_frame_delim
            ));
            break;
        case PHPSPY_TRACE_EVENT_STACK_END:
            if (opt_filter_re) {
                rv = regexec(opt_filter_re, udata->buf, 0, NULL, 0);
                if (opt_filter_negate == 0 && rv != 0) break;
                if (opt_filter_negate != 0 && rv == 0) break;
            }
            try(rv, event_handler_fout_snprintf(
                &udata->cur,
                &udata->rem,
                &len,
                "%s",
                opt_trace_delim
            ));
            write_len = (udata->cur - udata->buf) - 1;
            if (write_len < 1) {
                /* nothing to write */
            } else if (write(udata->fdout, udata->buf, write_len) != write_len) {
                fprintf(stderr, "event_handler_fout: Write failed (%s)\n", errno != 0 ? strerror(errno) : "partial");
                return 1;
            }
            break;
        case PHPSPY_TRACE_EVENT_ERROR:
            fprintf(stderr, "%s\n", context->event.error);
            break;
        case PHPSPY_TRACE_EVENT_DEINIT:
            fclose(udata->fout);
            free(udata);
            break;
    }
    return 0;
}

static int event_handler_fout_snprintf(char **s, size_t *n, size_t *ret_len, const char *fmt, ...) {
    int len;
    va_list vl;
    va_start(vl, fmt);
    len = vsnprintf(*s, *n, fmt, vl);
    va_end(vl);
    if (len < 0 || (size_t)len >= *n) {
        fprintf(stderr, "event_handler_fout_snprintf: Not enough space in buffer; truncating\n");
        return 1;
    }
    *s += len;
    *n -= len;
    *ret_len = (size_t)len;
    return 0;
}

static int event_handler_fout_open(FILE **fout) {
    int tfd;
    tfd = -1;
    if (strcmp(opt_path_output, "-") == 0) {
        tfd = dup(STDOUT_FILENO);
        *fout = fdopen(tfd, "w");
    } else {
        *fout = fopen(opt_path_output, "w");
    }
    if (!*fout) {
        perror("fopen");
        if (tfd != -1) close(tfd);
        return 1;
    }
    return 0;
}

#include "phpspy.h"

typedef struct event_handler_fout_udata_s {
    FILE *fout;
    char buf[4096];
    char *cur;
    size_t rem_len;
} event_handler_fout_udata_t;

static int event_handler_fout_open(FILE **fout);

int event_handler_fout(struct trace_context_s *context, int event_type) {
    int rv;
    FILE *fout;
    size_t len;
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
            udata->cur = udata->buf;
            udata->rem_len = sizeof(udata->buf) - 1;
            context->event_udata = udata;
            break;
        case PHPSPY_TRACE_EVENT_STACK_BEGIN:
            udata->cur = udata->buf;
            udata->rem_len = sizeof(udata->buf) - 1;
            break;
        case PHPSPY_TRACE_EVENT_FRAME:
            frame = &context->event.frame;
            len = snprintf(
                udata->cur,
                udata->rem_len + 1,
                "%d %.*s%s%.*s %.*s:%d%s",
                frame->depth,
                (int)frame->class_len, frame->class,
                frame->class_len > 0 ? "::" : "",
                (int)frame->func_len, frame->func,
                (int)frame->file_len, frame->file,
                frame->lineno,
                opt_frame_delim
            );
            len = PHPSPY_MIN(udata->rem_len, len);
            break;
        case PHPSPY_TRACE_EVENT_VARPEEK:
            fprintf(
                udata->fout,
                "# varpeek %s@%s = %s%s",
                context->event.varpeek.entry->varname,
                context->event.varpeek.entry->filename_lineno,
                context->event.varpeek.zval_str,
                opt_frame_delim
            );
            break;
        case PHPSPY_TRACE_EVENT_REQUEST:
            request = &context->event.request;
            len = snprintf(
                udata->cur,
                udata->rem_len + 1,
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
            );
            len = PHPSPY_MIN(udata->rem_len, len);
            break;
        case PHPSPY_TRACE_EVENT_MEM:
            len = snprintf(
                udata->cur,
                udata->rem_len + 1,
                "# mem %lu %lu%s",
                (uint64_t)context->event.mem.size,
                (uint64_t)context->event.mem.peak,
                opt_frame_delim
            );
            len = PHPSPY_MIN(udata->rem_len, len);
            break;
        case PHPSPY_TRACE_EVENT_STACK_END:
            if (opt_filter_re && regexec(opt_filter_re, udata->buf, 0, NULL, 0) != 0) {
                break;
            }
            fprintf(
                udata->fout,
                "%s%s",
                udata->buf,
                opt_trace_delim
            );
            break;
        case PHPSPY_TRACE_EVENT_ERROR:
            fprintf(stderr, "%s\n", context->event.error);
            break;
        case PHPSPY_TRACE_EVENT_DEINIT:
            fclose(udata->fout);
            free(udata);
            break;
    }
    if (len > 0) {
        udata->rem_len -= len;
        udata->cur += len;
    }
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
    setvbuf(*fout, NULL, _IOLBF, PIPE_BUF);
    return 0;
}

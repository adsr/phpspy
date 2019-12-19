#include "phpspy.h"

/*
 * $ ./phpspy  -j callgrind -o out.cg -- php -r 'for ($i=0; $i<5; $i++) { time_nanosleep(0, 1000 * 1000 * 500); usleep(500 * 1000); }'
 * process_vm_readv: No such process
 * $ ./gprof2dot.py -o out.dot -f callgrind out.cg && dot -Tsvg out.dot -o out.svg
 */

#define MAX_STACK_DEPTH 128

typedef struct {
    char loc_str[PHPSPY_STR_SIZE];
    trace_loc_t loc;
    uint64_t inclusive;
    uint64_t count;
    UT_hash_handle hh;
} callgrind_callee_t;

typedef struct {
    char loc_str[PHPSPY_STR_SIZE];
    trace_loc_t loc;
    uint64_t exclusive;
    callgrind_callee_t *callees;
    UT_hash_handle hh;
} callgrind_caller_t;

typedef struct {
    trace_loc_t self[MAX_STACK_DEPTH];
    trace_loc_t prev[MAX_STACK_DEPTH];
    char self_str[MAX_STACK_DEPTH][PHPSPY_STR_SIZE];
    char prev_str[MAX_STACK_DEPTH][PHPSPY_STR_SIZE];
    int self_len;
    int prev_len;
    FILE *fout;
    callgrind_caller_t *callers;
} callgrind_udata_t;

static int callgrind_open(FILE **fout);
static void callgrind_sprint_loc(char *str, trace_loc_t *loc);
static void callgrind_ingest_frame(callgrind_udata_t *udata, struct trace_context_s *context);
static void callgrind_digest_stack(callgrind_udata_t *udata);
static void callgrind_dump(callgrind_udata_t *udata);
static void callgrind_free(callgrind_udata_t *udata);
static int callgrind_sort_callers(callgrind_caller_t *a, callgrind_caller_t *b);
static int callgrind_sort_callees(callgrind_callee_t *a, callgrind_callee_t *b);

/*
 *  prev: (nil)
 *  self: main a b c        c.excl++
 *                          root.calls[main].incl++
 *                          main.calls[a].incl++    main.calls[a].count++
 *                          a.calls[b].incl++       a.calls[b].count++
 *                          b.calls[c].incl++       b.calls[c].count++
 *
 *  prev: main a b c
 *  self: main a b          b.excl++
 *                          root.calls[main].incl++
 *                          main.calls[a].incl++
 *                          a.calls[b].incl++
 *
 *  prev: main a b c
 *  self: main a b c d      d.excl++
 *                          root.calls[main].incl++
 *                          main.calls[a].incl++
 *                          a.calls[b].incl++
 *                          b.calls[c].incl++
 *                          c.calls[d].incl++       c.calls[d].count++
 *
 *  prev: main a b c d
 *  self: main a b c d      d.excl++
 *                          root.calls[main].incl++
 *                          main.calls[a].incl++
 *                          a.calls[b].incl++
 *                          b.calls[c].incl++
 *                          c.calls[d].incl++
 *
 *  prev: main a b c d
 *  self: main x y          y.excl++
 *                          root.calls[main].incl++
 *                          main.calls[x].incl++    main.calls[x].count++
 *                          x.calls[y].incl++       x.calls[y].count++
 */

int event_handler_callgrind(struct trace_context_s *context, int event_type) {
    callgrind_udata_t *udata;

    udata = (callgrind_udata_t*)context->event_udata;
    if (!udata && event_type != PHPSPY_TRACE_EVENT_INIT) {
        return 1;
    }
    switch (event_type) {
        case PHPSPY_TRACE_EVENT_INIT:
            udata = calloc(1, sizeof(callgrind_udata_t));
            if (callgrind_open(&udata->fout) != 0) {
                free(udata);
                return 1;
            }
            context->event_udata = udata;
            break;
        case PHPSPY_TRACE_EVENT_STACK_BEGIN:
            udata->self_len = 0;
            break;
        case PHPSPY_TRACE_EVENT_FRAME:
            callgrind_ingest_frame(udata, context);
            break;
        case PHPSPY_TRACE_EVENT_STACK_END:
            callgrind_digest_stack(udata);
            memcpy(udata->prev,     udata->self,     sizeof(trace_loc_t) * udata->self_len);
            memcpy(udata->prev_str, udata->self_str, PHPSPY_STR_SIZE * udata->self_len);
            udata->prev_len = udata->self_len;
            udata->self_len = 0;
            break;
        case PHPSPY_TRACE_EVENT_ERROR:
            log_error("%s\n", context->event.error);
            break;
        case PHPSPY_TRACE_EVENT_DEINIT:
            callgrind_dump(udata);
            callgrind_free(udata);
            free(udata);
            break;
    }
    return 0;
}

static void callgrind_ingest_frame(callgrind_udata_t *udata, struct trace_context_s *context) {
    if (udata->self_len >= MAX_STACK_DEPTH) {
        log_error("callgrind_ingest_frame: Exceeded max stack depth (%d); truncating\n", MAX_STACK_DEPTH);
        return;
    }
    memcpy(&udata->self[udata->self_len], &context->event.frame.loc, sizeof(trace_loc_t));
    callgrind_sprint_loc(udata->self_str[udata->self_len], &context->event.frame.loc);
    udata->self_len += 1;
}

static void callgrind_sprint_loc(char *str, trace_loc_t *loc) {
    int len;
    len = snprintf(str, PHPSPY_STR_SIZE,
        "%.*s%s%.*s %.*s:%d",
        (int)loc->class_len, loc->class,
        loc->class_len > 0 ? "::" : "",
        (int)loc->func_len, loc->func,
        (int)loc->file_len, loc->file,
        loc->lineno
    );
    if (len >= PHPSPY_STR_SIZE) {
        log_error("callgrind_sprint_loc: Exceeded max loc len (%d); truncating\n", PHPSPY_STR_SIZE);
    }
}

static void callgrind_digest_stack(callgrind_udata_t *udata) {
    callgrind_caller_t *caller, *prev_caller;
    callgrind_callee_t *callee;
    int i;

    prev_caller = NULL;

    for (i = udata->self_len - 1; i >= 0; i--) {

        /* Find or add caller */
        HASH_FIND_STR(udata->callers, udata->self_str[i], caller);
        if (!caller) {
            caller = calloc(1, sizeof(callgrind_caller_t));
            strcpy(caller->loc_str, udata->self_str[i]);
            memcpy(&caller->loc, &udata->self[i], sizeof(trace_loc_t));
            HASH_ADD_STR(udata->callers, loc_str, caller);
        }

        /* Increment exclusive cost if top of call stack */
        if (i == 0) {
            caller->exclusive += 1;
        }

        if (prev_caller) {
            /* Find or add callee in previous caller */
            HASH_FIND_STR(prev_caller->callees, udata->self_str[i], callee);
            if (!callee) {
                callee = calloc(1, sizeof(callgrind_callee_t));
                strcpy(callee->loc_str, udata->self_str[i]);
                memcpy(&callee->loc, &udata->self[i], sizeof(trace_loc_t));
                HASH_ADD_STR(prev_caller->callees, loc_str, callee);
            }

            /* Increment inclusive cost */
            callee->inclusive += 1;

            /* Increment call count if frame looks different from frame in prev trace */
            if (i >= udata->prev_len
                || strcmp(udata->self_str[i], udata->prev_str[i]) != 0
            ) {
                callee->count += 1;
            }
        }
        /* Remember previous caller */
        prev_caller = caller;
    }
}

static void callgrind_dump(callgrind_udata_t *udata) {
    callgrind_caller_t *caller, *caller_tmp;
    callgrind_callee_t *callee, *callee_tmp;

    fprintf(udata->fout, "# callgrind format\n");
    fprintf(udata->fout, "version: 1\n");
    fprintf(udata->fout, "creator: phpspy\n");
    fprintf(udata->fout, "events: Samples\n");

    HASH_SORT(udata->callers, callgrind_sort_callers);
    HASH_ITER(hh, udata->callers, caller, caller_tmp) {
        fprintf(udata->fout, "\n");
        fprintf(udata->fout, "fl=%.*s\n", (int)caller->loc.file_len, caller->loc.file);
        fprintf(udata->fout, "fn=%.*s%s%.*s\n", (int)caller->loc.class_len, caller->loc.class, caller->loc.class_len > 0 ? "::" : "", (int)caller->loc.func_len, caller->loc.func);
        fprintf(udata->fout, "%d %ld\n", caller->loc.lineno, caller->exclusive);

        HASH_SORT(caller->callees, callgrind_sort_callees);
        HASH_ITER(hh, caller->callees, callee, callee_tmp) {
            fprintf(udata->fout, "\n");
            fprintf(udata->fout, "cfl=%.*s\n", (int)callee->loc.file_len, callee->loc.file);
            fprintf(udata->fout, "cfn=%.*s%s%.*s\n", (int)callee->loc.class_len, callee->loc.class, callee->loc.class_len > 0 ? "::" : "", (int)callee->loc.func_len, callee->loc.func);
            fprintf(udata->fout, "calls=%ld %d\n", callee->count, callee->loc.lineno);
            fprintf(udata->fout, "%d %ld\n", caller->loc.lineno, callee->inclusive);
        }
    }
}

static void callgrind_free(callgrind_udata_t *udata) {
    callgrind_caller_t *caller, *caller_tmp;
    callgrind_callee_t *callee, *callee_tmp;

    HASH_ITER(hh, udata->callers, caller, caller_tmp) {
        HASH_ITER(hh, caller->callees, callee, callee_tmp) {
            HASH_DEL(caller->callees, callee);
            free(callee);
        }
        HASH_DEL(udata->callers, caller);
        free(caller);
    }
}

static int callgrind_sort_callers(callgrind_caller_t *a, callgrind_caller_t *b) {
    return strcmp(a->loc_str, b->loc_str);
}

static int callgrind_sort_callees(callgrind_callee_t *a, callgrind_callee_t *b) {
    return strcmp(a->loc_str, b->loc_str);
}

static int callgrind_open(FILE **fout) {
    /* TODO consolidate -o -O -E flags as `-o fn` == "fn[.pid].(out|err) */
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

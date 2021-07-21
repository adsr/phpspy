#define try_copy_proc_mem(__what, __raddr, __laddr, __size) \
    try(rv, copy_proc_mem(context->target.pid, (__what), (__raddr), (__laddr), (__size)))

static int trace_stack(trace_context_t *context, zend_execute_data *remote_execute_data, int *depth);
static int trace_request_info(trace_context_t *context);
static int trace_memory_info(trace_context_t *context);
static int trace_globals(trace_context_t *context);
static int trace_locals(trace_context_t *context, zend_op *zop, zend_execute_data *remote_execute_data, zend_op_array *op_array, char *file, int file_len);

static int copy_executor_globals(trace_context_t *context, zend_executor_globals *executor_globals);
static int copy_zarray_bucket(trace_context_t *context, zend_array *rzarray, const char *key, Bucket *lbucket);

static int sprint_zstring(trace_context_t *context, const char *what, zend_string *lzstring, char *buf, size_t buf_size, size_t *buf_len);
static int sprint_zval(trace_context_t *context, zval *lzval, char *buf, size_t buf_size, size_t *buf_len);
static int sprint_zarray(trace_context_t *context, zend_array *rzarray, char *buf, size_t buf_size, size_t *buf_len);
static int sprint_zarray_val(trace_context_t *context, zend_array *rzarray, const char *key, char *buf, size_t buf_size, size_t *buf_len);
static int sprint_zarray_bucket(trace_context_t *context, Bucket *lbucket, char *buf, size_t buf_size, size_t *buf_len);

/*********************
    Trace functions
 *********************/

/**
 * Sample a single execution trace.
 *
 * @param context Trace context
 *
 * @return int Status code
 */
static int do_trace(trace_context_t *context) {
    int rv, depth;
    zend_executor_globals executor_globals;

    try(rv, copy_executor_globals(context, &executor_globals));
    try(rv, context->event_handler(context, PHPSPY_TRACE_EVENT_STACK_BEGIN));

    rv = PHPSPY_OK;
    do {
        #define maybe_break_on_err() do {                   \
            if ((rv & PHPSPY_ERR_PID_DEAD) != 0) {          \
                break;                                      \
            } else if ((rv & PHPSPY_ERR_BUF_FULL) != 0) {   \
                break;                                      \
            } else if (!opt_continue_on_error) {            \
                break;                                      \
            }                                               \
        } while(0)

        rv |= trace_stack(context, executor_globals.current_execute_data, &depth);
        maybe_break_on_err();
        if (depth < 1) break;

        if (opt_capture_req) {
            rv |= trace_request_info(context);
            maybe_break_on_err();
        }

        if (opt_capture_mem) {
            rv |= trace_memory_info(context);
            maybe_break_on_err();
        }

        if (HASH_CNT(hh, glopeek_map) > 0) {
            rv |= trace_globals(context);
            maybe_break_on_err();
        }

        #undef maybe_break_on_err
    } while (0);

    if (rv == PHPSPY_OK || opt_continue_on_error) {
        try(rv, context->event_handler(context, PHPSPY_TRACE_EVENT_STACK_END));
    }

    return PHPSPY_OK;
}

/**
 * Iterate through the callstack and trace each frame with the PHPSPY_TRACE_EVENT_FRAME event.
 *
 * @param context             Trace context
 * @param remote_execute_data Remote execute_data
 * @param depth               Stack depth output
 *
 * @return int Status code
 */
static int trace_stack(trace_context_t *context, zend_execute_data *remote_execute_data, int *depth) {
    int rv;
    zend_execute_data execute_data;
    zend_function zfunc;
    zend_string zstring;
    zend_class_entry zce;
    zend_op zop;
    trace_target_t *target;
    trace_frame_t *frame;

    target = &context->target;
    frame = &context->event.frame;
    *depth = 0;

    while (remote_execute_data && *depth != opt_max_stack_depth) { /* TODO make options struct */
        memset(&execute_data, 0, sizeof(execute_data));
        memset(&zfunc, 0, sizeof(zfunc));
        memset(&zstring, 0, sizeof(zstring));
        memset(&zce, 0, sizeof(zce));
        memset(&zop, 0, sizeof(zop));

        /* TODO reduce number of copy calls */
        try_copy_proc_mem("execute_data", remote_execute_data, &execute_data, sizeof(execute_data));
        try_copy_proc_mem("zfunc", execute_data.func, &zfunc, sizeof(zfunc));
        if (zfunc.common.function_name) {
            try(rv, sprint_zstring(context, "function_name", zfunc.common.function_name, frame->loc.func, sizeof(frame->loc.func), &frame->loc.func_len));
        } else {
            frame->loc.func_len = snprintf(frame->loc.func, sizeof(frame->loc.func), "<main>");
        }
        if (zfunc.common.scope) {
            try_copy_proc_mem("zce", zfunc.common.scope, &zce, sizeof(zce));
            try(rv, sprint_zstring(context, "class_name", zce.name, frame->loc.class, sizeof(frame->loc.class), &frame->loc.class_len));
        } else {
            frame->loc.class[0] = '\0';
            frame->loc.class_len = 0;
        }
        if (zfunc.type == 2) {
            try(rv, sprint_zstring(context, "filename", zfunc.op_array.filename, frame->loc.file, sizeof(frame->loc.file), &frame->loc.file_len));
            frame->loc.lineno = zfunc.op_array.line_start;
            /* TODO add comments */
            if (HASH_CNT(hh, varpeek_map) > 0) {
                if (copy_proc_mem(target->pid, "opline", (void*)execute_data.opline, &zop, sizeof(zop)) == PHPSPY_OK) {
                    trace_locals(context, &zop, remote_execute_data, &zfunc.op_array, frame->loc.file, frame->loc.file_len);
                }
            }
        } else {
            frame->loc.file_len = snprintf(frame->loc.file, sizeof(frame->loc.file), "<internal>");
            frame->loc.lineno = -1;
        }
        frame->depth = *depth;
        try(rv, context->event_handler(context, PHPSPY_TRACE_EVENT_FRAME));
        remote_execute_data = execute_data.prev_execute_data;
        *depth += 1;
    }

    return PHPSPY_OK;
}

/**
 * Trace request info with the PHPSPY_TRACE_EVENT_REQUEST event.
 *
 * @param context Trace context
 *
 * @return int Status code
 */
static int trace_request_info(trace_context_t *context) {
    int rv;
    sapi_globals_struct sapi_globals;
    trace_target_t *target;
    trace_request_t *request;

    memset(&sapi_globals, 0, sizeof(sapi_globals));
    request = &context->event.request;
    target = &context->target;

    try_copy_proc_mem("sapi_globals", (void*)target->sapi_globals_addr, &sapi_globals, sizeof(sapi_globals));
    #define try_copy_sapi_global_field(__field, __local) do {                                                   \
        if ((opt_capture_req_ ## __local) && sapi_globals.request_info.__field) {                               \
            try_copy_proc_mem(#__field, sapi_globals.request_info.__field, request->__local, PHPSPY_STR_SIZE);  \
        } else {                                                                                                \
            request->__local[0] = '-';                                                                          \
            request->__local[1] = '\0';                                                                         \
        }                                                                                                       \
    } while (0)
    try_copy_sapi_global_field(query_string, qstring);
    try_copy_sapi_global_field(cookie_data, cookie);
    try_copy_sapi_global_field(request_uri, uri);
    try_copy_sapi_global_field(path_translated, path);
    #undef try_copy_sapi_global_field

    request->ts = sapi_globals.global_request_time;

    try(rv, context->event_handler(context, PHPSPY_TRACE_EVENT_REQUEST));

    return PHPSPY_OK;
}

/**
 * Trace memory usage with the PHPSPY_TRACE_EVENT_MEM event.
 *
 * @param context Trace context
 *
 * @return int Status code
 */
static int trace_memory_info(trace_context_t *context) {
    #ifdef USE_ZEND
    (void)context;
    return PHPSPY_ERR; /* zend_alloc_globals is not public */
    #else
    int rv;
    zend_mm_heap mm_heap;
    zend_alloc_globals alloc_globals;
    trace_target_t *target;

    memset(&mm_heap, 0, sizeof(mm_heap));
    alloc_globals.mm_heap = NULL;
    target = &context->target;

    try_copy_proc_mem("alloc_globals", (void*)target->alloc_globals_addr, &alloc_globals, sizeof(alloc_globals));
    try_copy_proc_mem("mm_heap", alloc_globals.mm_heap, &mm_heap, sizeof(mm_heap));
    context->event.mem.size = mm_heap.size;
    context->event.mem.peak = mm_heap.peak;

    try(rv, context->event_handler(context, PHPSPY_TRACE_EVENT_MEM));

    return PHPSPY_OK;

    #endif
}

/**
 * Trace global variable values with the PHPSPY_TRACE_EVENT_GLOPEEK event.
 *
 * @param context Trace context
 *
 * @return int Status code
 */
static int trace_globals(trace_context_t *context) {
    int rv;
    glopeek_entry_t *gentry, *gentry_tmp;
    zend_array *garray;
    zend_array *symtable;
    Bucket lbucket;

    /* Find the remote address of executor_globals.symbol_table */
    symtable = (zend_array *)(context->target.executor_globals_addr + offsetof(zend_executor_globals, symbol_table));

    HASH_ITER(hh, glopeek_map, gentry, gentry_tmp) {

        /* Point garray at the zend_array where this global variable resides */
        if (gentry->gloname[0]) {
            try(rv, copy_zarray_bucket(context, symtable, gentry->gloname, &lbucket));
            garray = lbucket.val.value.arr;
        } else {
            garray = symtable;
        }

        /* Print the element within the array */

        rv = sprint_zarray_val(context, garray, gentry->varname, context->buf, sizeof(context->buf), &context->buf_len);

        if (rv == PHPSPY_OK) {
            context->event.glopeek.gentry = gentry;
            context->event.glopeek.zval_str = context->buf;
            try(rv, context->event_handler(context, PHPSPY_TRACE_EVENT_GLOPEEK));
        }
    }

    return PHPSPY_OK;
}

/**
 * Trace local variable values with the PHPSPY_TRACE_EVENT_VARPEEK event.
 *
 * @param context             Trace context
 * @param zop                 Local zend_op
 * @param remote_execute_data Remote execute_data
 * @param op_array            Local zend_op_array
 * @param file                Current filename
 * @param file_len            Current filename length
 *
 * @return int Status code
 */
static int trace_locals(trace_context_t *context, zend_op *zop, zend_execute_data *remote_execute_data, zend_op_array *op_array, char *file, int file_len) {
    int rv, i, num_vars_found, num_vars_peeking;
    char tmp[PHPSPY_STR_SIZE];
    size_t tmp_len;
    zend_string *zstrp;
    varpeek_entry_t *entry;
    varpeek_var_t *var;
    char varpeek_key[PHPSPY_STR_SIZE];
    zval zv;

    snprintf(varpeek_key, sizeof(varpeek_key), "%.*s:%d", file_len, file, zop->lineno);
    HASH_FIND_STR(varpeek_map, varpeek_key, entry);
    if (!entry) return PHPSPY_OK;

    num_vars_found = 0;
    num_vars_peeking = HASH_CNT(hh, entry->varmap);

    for (i = 0; i < op_array->last_var; i++) {
        try_copy_proc_mem("var", op_array->vars + i, &zstrp, sizeof(zstrp));
        try(rv, sprint_zstring(context, "var", zstrp, tmp, sizeof(tmp), &tmp_len));
        HASH_FIND(hh, entry->varmap, tmp, tmp_len, var);
        if (!var) continue;
        num_vars_found += 1;
        /* See ZEND_CALL_VAR_NUM macro in php-src */
        try_copy_proc_mem("zval", ((zval*)(remote_execute_data)) + ((int)(5 + i)), &zv, sizeof(zv));
        try(rv, sprint_zval(context, &zv, tmp, sizeof(tmp), &tmp_len));
        context->event.varpeek.entry = entry;
        context->event.varpeek.var = var;
        context->event.varpeek.zval_str = tmp;
        try(rv, context->event_handler(context, PHPSPY_TRACE_EVENT_VARPEEK));
        if (num_vars_found >= num_vars_peeking) break;
    }

    return PHPSPY_OK;
}

/********************
    Copy functions
 ********************/

/**
 * Copy executor_globals from the remote process to local memory.
 *
 * @param context          Trace context
 * @param executor_globals Local destination executor_globals
 *
 * @return int Status code
 */
static int copy_executor_globals(trace_context_t *context, zend_executor_globals *executor_globals) {
    int rv;
    executor_globals->current_execute_data = NULL;
    try_copy_proc_mem("executor_globals", (void*)context->target.executor_globals_addr, executor_globals, sizeof(*executor_globals));
    return PHPSPY_OK;
}

/**
 * Copy an element from a remote zend_array to a Bucket in local memory.
 *
 * @param context Trace context
 * @param rzarray Remote zend_array
 * @param key     Array key of element
 * @param lbucket Local Bucket to write to
 *
 * @return int Status code
 */
static int copy_zarray_bucket(trace_context_t *context, zend_array *rzarray, const char *key, Bucket *lbucket) {
    int rv;
    zend_array lzarray;
    uint32_t hash_table_size;
    uint64_t hash_val;
    uint32_t hash_index;
    uint32_t hash_table_val;
    uint32_t *hash_bucket;
    char tmp_key[PHPSPY_STR_SIZE];
    size_t tmp_len;

    try_copy_proc_mem("array", rzarray, &lzarray, sizeof(lzarray));

    hash_val = phpspy_zend_inline_hash_func(key, strlen(key));
    hash_table_size = (uint32_t)(-1 * (int32_t)lzarray.nTableMask);
    hash_index = hash_val % hash_table_size;

    try_copy_proc_mem("hash_table_val", ((uint32_t*)lzarray.arData) - hash_table_size + hash_index, &hash_table_val, sizeof(uint32_t));

    hash_bucket = &hash_table_val;

    do {
        if (*hash_bucket == (uint32_t)-1) return PHPSPY_ERR;

        /* Copy the next bucket from array data */
        try_copy_proc_mem("bucket", lzarray.arData + *hash_bucket, lbucket, sizeof(Bucket));

        if (lbucket->key == NULL) {
            break;
        }

        /* On hash collision, advance to the next bucket */
        try(rv, sprint_zstring(context, "array_key", lbucket->key, tmp_key, sizeof(tmp_key), &tmp_len));

        if (strcmp(key, tmp_key) == 0) {
            hash_bucket = NULL;
        } else {
            hash_bucket = &lbucket->val.u2.next;
        }
    } while (hash_bucket);

    return PHPSPY_OK;
}

/*********************
    Print functions
 *********************/

/**
 * Print the value of a remote zend_string to a string buffer.
 *
 * @param context  Trace context
 * @param what     Memory copy message
 * @param rzstring Remote zstring
 * @param buf      String buffer to write to
 * @param buf_size Size of string buffer
 * @param buf_len  Length of written string
 *
 * @return int Status code
 */
static int sprint_zstring(trace_context_t *context, const char *what, zend_string *rzstring, char *buf, size_t buf_size, size_t *buf_len) {
    int rv;
    zend_string lzstring;

    *buf = '\0';
    *buf_len = 0;
    try_copy_proc_mem(what, rzstring, &lzstring, sizeof(lzstring));
    *buf_len = PHPSPY_MIN(lzstring.len, PHPSPY_MAX(1, buf_size)-1);
    try_copy_proc_mem(what, ((char*)rzstring) + offsetof(zend_string, val), buf, *buf_len);
    *(buf + (int)*buf_len) = '\0';

    return PHPSPY_OK;
}

/**
 * Print the value of a local zval to a string buffer. For some types, this entails copying remote value data.
 *
 * @param context  Trace context
 * @param lzval    Local zval
 * @param buf      String buffer to write to
 * @param buf_size Size of string buffer
 * @param buf_len  Length of written string
 *
 * @return int Status code
 */
static int sprint_zval(trace_context_t *context, zval *lzval, char *buf, size_t buf_size, size_t *buf_len) {
    int rv;
    int type;
    type = (int)lzval->u1.v.type;
    switch (type) {
        case IS_LONG:
            snprintf(buf, buf_size, "%ld", lzval->value.lval);
            *buf_len = strlen(buf);
            break;
        case IS_DOUBLE:
            snprintf(buf, buf_size, "%f", lzval->value.dval);
            *buf_len = strlen(buf);
            break;
        case IS_STRING:
            try(rv, sprint_zstring(context, "zval", lzval->value.str, buf, buf_size, buf_len));
            break;
        case IS_ARRAY:
            try(rv, sprint_zarray(context, lzval->value.arr, buf, buf_size, buf_len));
            break;
        default:
            /* TODO handle other zval types */
            /* fprintf(context->fout, "value not supported, found type: %d\n", type); */
            return PHPSPY_ERR;
    }
    return PHPSPY_OK;
}

/**
 * Print a comma-separated list of zend_array values to a string buffer.
 *
 * @param context  Trace context
 * @param rzarray  Remote zend_array
 * @param buf      String buffer to write to
 * @param buf_size Size of string buffer
 * @param buf_len  Length of written string
 *
 * @return int Status code
 */
static int sprint_zarray(trace_context_t *context, zend_array *rzarray, char *buf, size_t buf_size, size_t *buf_len) {
    int rv;
    int i;
    int array_len;
    size_t tmp_len;
    Bucket buckets[PHPSPY_MAX_ARRAY_BUCKETS];
    zend_array lzarray;
    char *obuf;

    obuf = buf;
    try_copy_proc_mem("array", rzarray, &lzarray, sizeof(lzarray));

    array_len = PHPSPY_MIN(lzarray.nNumOfElements, PHPSPY_MAX_ARRAY_BUCKETS);
    try_copy_proc_mem("buckets", lzarray.arData, buckets, sizeof(Bucket) * array_len);

    for (i = 0; i < array_len; i++) {
        try(rv, sprint_zarray_bucket(context, buckets + i, buf, buf_size, &tmp_len));
        buf_size -= tmp_len;
        buf += tmp_len;

        /* TODO Introduce a string class to clean this silliness up */
        if (buf_size >= 2) {
            *buf = ',';
            --buf_size;
            ++buf;
        }
    }

    *buf_len = (size_t)(buf - obuf);

    return PHPSPY_OK;
}

/**
 * Print a single zend_array value to a string buffer.
 *
 * @param context  Trace context
 * @param rzarray  Remote zend_array
 * @param key      Array key of element to print
 * @param buf      String buffer to write to
 * @param buf_size Size of string buffer
 * @param buf_len  Length of written string
 *
 * @return int Status code
 */
static int sprint_zarray_val(trace_context_t *context, zend_array *rzarray, const char *key, char *buf, size_t buf_size, size_t *buf_len) {
    int rv;
    Bucket bucket;

    try(rv, copy_zarray_bucket(context, rzarray, key, &bucket));
    try(rv, sprint_zval(context, &bucket.val, buf, buf_size, buf_len));

    return PHPSPY_OK;
}

/**
 * Print a zend_array Bucket, with its key, to a string buffer.
 *
 * @param context  Trace context
 * @param lbucket  Local Bucket
 * @param buf      String buffer to write to
 * @param buf_size Size of string buffer
 * @param buf_len  Length of written string
 *
 * @return int Status code
 */
static int sprint_zarray_bucket(trace_context_t *context, Bucket *lbucket, char *buf, size_t buf_size, size_t *buf_len) {
    int rv;
    char tmp_key[PHPSPY_STR_SIZE];
    size_t tmp_len;
    char *obuf;

    obuf = buf;

    if (lbucket->key != NULL) {
        try(rv, sprint_zstring(context, "array_key", lbucket->key, tmp_key, sizeof(tmp_key), &tmp_len));

        /* TODO Introduce a string class to clean this silliness up */
        if (buf_size > tmp_len + 1 + 1) {
            snprintf(buf, buf_size, "%s=", tmp_key);
            buf_size -= tmp_len + 1;
            buf += tmp_len + 1;
        }
    }

    try(rv, sprint_zval(context, &lbucket->val, buf, buf_size, &tmp_len));
    buf += tmp_len;

    *buf_len = (size_t)(buf - obuf);
    return PHPSPY_OK;
}

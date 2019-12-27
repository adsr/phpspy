#define try_copy_proc_mem(__what, __raddr, __laddr, __size) \
    try(rv, copy_proc_mem(context->target.pid, (__what), (__raddr), (__laddr), (__size)))

static int copy_executor_globals(trace_context_t *context, zend_executor_globals *executor_globals);
static int copy_stack(trace_context_t *context, zend_execute_data *remote_execute_data, int *depth);
static int copy_request_info(trace_context_t *context);
static int copy_memory_info(trace_context_t *context);
static int copy_globals(trace_context_t *context);
static int copy_locals(trace_context_t *context, zend_op *zop, zend_execute_data *remote_execute_data, zend_op_array *op_array, char *file, int file_len);
static int copy_zstring(trace_context_t *context, const char *what, zend_string *rzstring, char *buf, size_t buf_size, size_t *buf_len);
static int copy_zval(trace_context_t *context, zval *local_zval, char *buf, size_t buf_size, size_t *buf_len);
static int copy_zarray(trace_context_t *context, zend_array *local_arr, char *buf, size_t buf_size, size_t *buf_len, char *single_key);

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

        rv |= copy_stack(context, executor_globals.current_execute_data, &depth);
        maybe_break_on_err();
        if (depth < 1) break;

        if (opt_capture_req) {
            rv |= copy_request_info(context);
            maybe_break_on_err();
        }

        if (opt_capture_mem) {
            rv |= copy_memory_info(context);
            maybe_break_on_err();
        }

        if (HASH_CNT(hh, glopeek_map) > 0) {
            rv |= copy_globals(context);
            maybe_break_on_err();
        }

        #undef maybe_break_on_err
    } while (0);

    if (rv == PHPSPY_OK || opt_continue_on_error) {
        try(rv, context->event_handler(context, PHPSPY_TRACE_EVENT_STACK_END));
    }

    return PHPSPY_OK;
}

static int copy_executor_globals(trace_context_t *context, zend_executor_globals *executor_globals) {
    int rv;
    executor_globals->current_execute_data = NULL;
    try_copy_proc_mem("executor_globals", (void*)context->target.executor_globals_addr, executor_globals, sizeof(*executor_globals));
    return PHPSPY_OK;
}

static int copy_stack(trace_context_t *context, zend_execute_data *remote_execute_data, int *depth) {
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
            try(rv, copy_zstring(context, "function_name", zfunc.common.function_name, frame->loc.func, sizeof(frame->loc.func), &frame->loc.func_len));
        } else {
            frame->loc.func_len = snprintf(frame->loc.func, sizeof(frame->loc.func), "<main>");
        }
        if (zfunc.common.scope) {
            try_copy_proc_mem("zce", zfunc.common.scope, &zce, sizeof(zce));
            try(rv, copy_zstring(context, "class_name", zce.name, frame->loc.class, sizeof(frame->loc.class), &frame->loc.class_len));
        } else {
            frame->loc.class[0] = '\0';
            frame->loc.class_len = 0;
        }
        if (zfunc.type == 2) {
            try(rv, copy_zstring(context, "filename", zfunc.op_array.filename, frame->loc.file, sizeof(frame->loc.file), &frame->loc.file_len));
            frame->loc.lineno = zfunc.op_array.line_start;
            /* TODO add comments */
            if (HASH_CNT(hh, varpeek_map) > 0) {
                if (copy_proc_mem(target->pid, "opline", execute_data.opline, &zop, sizeof(zop)) == PHPSPY_OK) {
                    copy_locals(context, &zop, remote_execute_data, &zfunc.op_array, frame->loc.file, frame->loc.file_len);
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

static int copy_request_info(trace_context_t *context) {
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

static int copy_memory_info(trace_context_t *context) {
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
}

static int copy_globals(trace_context_t *context) {
    int rv;
    php_core_globals core_globals;
    glopeek_entry_t *gentry, *gentry_tmp;
    trace_target_t *target;

    memset(&core_globals, 0, sizeof(php_core_globals));
    target = &context->target;

    try_copy_proc_mem("core_globals", (void*)target->core_globals_addr, &core_globals, sizeof(core_globals));
    HASH_ITER(hh, glopeek_map, gentry, gentry_tmp) {
        try(rv, copy_zarray(context, core_globals.http_globals[gentry->index].value.arr, context->buf, sizeof(context->buf), &context->buf_len, gentry->varname));
        context->event.glopeek.gentry = gentry;
        context->event.glopeek.zval_str = context->buf;
        try(rv, context->event_handler(context, PHPSPY_TRACE_EVENT_GLOPEEK));
    }

    return PHPSPY_OK;
}

static int copy_locals(trace_context_t *context, zend_op *zop, zend_execute_data *remote_execute_data, zend_op_array *op_array, char *file, int file_len) {
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
        try(rv, copy_zstring(context, "var", zstrp, tmp, sizeof(tmp), &tmp_len));
        HASH_FIND(hh, entry->varmap, tmp, tmp_len, var);
        if (!var) continue;
        num_vars_found += 1;
        /* See ZEND_CALL_VAR_NUM macro in php-src */
        try_copy_proc_mem("zval", ((zval*)(remote_execute_data)) + ((int)(5 + i)), &zv, sizeof(zv));
        try(rv, copy_zval(context, &zv, tmp, sizeof(tmp), &tmp_len));
        context->event.varpeek.entry = entry;
        context->event.varpeek.var = var;
        context->event.varpeek.zval_str = tmp;
        try(rv, context->event_handler(context, PHPSPY_TRACE_EVENT_VARPEEK));
        if (num_vars_found >= num_vars_peeking) break;
    }

    return PHPSPY_OK;
}

static int copy_zstring(trace_context_t *context, const char *what, zend_string *rzstring, char *buf, size_t buf_size, size_t *buf_len) {
    int rv;
    zend_string lzstring;
    *buf = '\0';
    *buf_len = 0;
    try_copy_proc_mem(what, rzstring, &lzstring, sizeof(lzstring));
    *buf_len = PHPSPY_MIN(lzstring.len, PHPSPY_MAX(1, buf_size)-1);
    try_copy_proc_mem(what, ((char*)rzstring) + zend_string_val_offset, buf, *buf_len);
    *(buf + (int)*buf_len) = '\0';
    return PHPSPY_OK;
}

static int copy_zval(trace_context_t *context, zval *local_zval, char *buf, size_t buf_size, size_t *buf_len) {
    int rv;
    int type;
    type = (int)local_zval->u1.v.type;
    switch (type) {
        case IS_LONG:
            snprintf(buf, buf_size, "%ld", local_zval->value.lval);
            *buf_len = strlen(buf);
            break;
        case IS_DOUBLE:
            snprintf(buf, buf_size, "%f", local_zval->value.dval);
            *buf_len = strlen(buf);
            break;
        case IS_STRING:
            try(rv, copy_zstring(context, "zval", local_zval->value.str, buf, buf_size, buf_len));
            break;
        case IS_ARRAY:
            try(rv, copy_zarray(context, local_zval->value.arr, buf, buf_size, buf_len, NULL));
            break;
        default:
            /* TODO handle other zval types */
            /* fprintf(context->fout, "value not supported, found type: %d\n", type); */
            return PHPSPY_ERR;
    }
    return PHPSPY_OK;
}

static int copy_zarray(trace_context_t *context, zend_array *local_arr, char *buf, size_t buf_size, size_t *buf_len, char *single_key) {
    int rv;
    int i;
    int array_len;
    size_t tmp_len;
    Bucket buckets[PHPSPY_MAX_BUCKETS];
    zend_array array;
    char *obuf;

    obuf = buf;
    try_copy_proc_mem("array", local_arr, &array, sizeof(array));
    array_len = PHPSPY_MIN(array.nNumOfElements, PHPSPY_MAX_BUCKETS);

    try_copy_proc_mem("buckets", array.arData, buckets, sizeof(Bucket) * array_len);
    for (i = 0; i < array_len; i++) {
        if (single_key) {
            if (buckets[i].key == 0) {
                continue;
            }
            try(rv, copy_zstring(context, "array_key", buckets[i].key, buf, buf_size, &tmp_len));
            if (strncmp(buf, single_key, tmp_len) != 0 || tmp_len != strlen(single_key)) {
                continue;
            }
            try(rv, copy_zval(context, &buckets[i].val, buf, buf_size, &tmp_len));
            *buf_len = (size_t)(buf - obuf);
            return PHPSPY_OK;
        } else {
            if (buckets[i].key != 0) {
                try(rv, copy_zstring(context, "array_key", buckets[i].key, buf, buf_size, &tmp_len));
                buf_size -= tmp_len;
                buf += tmp_len;

                snprintf(buf, buf_size, "=");
                tmp_len = strlen(buf);
                buf_size -= tmp_len;
                buf += tmp_len;
            }
            try(rv, copy_zval(context, &buckets[i].val, buf, buf_size, &tmp_len));
            buf_size -= tmp_len;
            buf += tmp_len;

            snprintf(buf, buf_size, ",");
            tmp_len = strlen(buf);
            buf_size -= tmp_len;
            buf += tmp_len;
        }
    }

    /* If we get here when looking for a single_key, we have lost */
    if (single_key) return PHPSPY_ERR;

    *buf_len = (size_t)(buf - obuf);
    return PHPSPY_OK;
}

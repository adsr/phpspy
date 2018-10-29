#define try_copy_proc_mem(__what, __raddr, __laddr, __size) \
    try(rv, copy_proc_mem(context, (__what), (__raddr), (__laddr), (__size)))

static int varpeek_find(trace_context_t *context, varpeek_entry_t *varpeek_map, zend_execute_data *local_execute_data, zend_execute_data *remote_execute_data, zend_op_array *op_array, char *file);
static int copy_zstring(trace_context_t *context, const char *what, zend_string *rzstring, char *buf, size_t buf_size, size_t *buf_len);
static int copy_zval(trace_context_t *context, zval *local_zval, char *buf, size_t buf_size, size_t *buf_len);
static int copy_zarray(trace_context_t *context, zend_array *local_arr, char *buf, size_t buf_size, size_t *buf_len);

static int do_trace(trace_context_t *context) {
    int depth;
    int rv;
    zend_execute_data *remote_execute_data;
    zend_executor_globals executor_globals;
    zend_execute_data execute_data;
    zend_class_entry zce;
    zend_function zfunc;
    zend_string zstring;
    sapi_globals_struct sapi_globals;
    trace_target_t *target;
    trace_frame_t *frame;
    trace_request_t *request;

    target = &context->target;
    frame = &context->event.frame;

    depth = 0;

    executor_globals.current_execute_data = NULL;
    try_copy_proc_mem("executor_globals", (void*)target->executor_globals_addr, &executor_globals, sizeof(executor_globals));
    remote_execute_data = executor_globals.current_execute_data;

    /* TODO reduce number of copy calls */
    context->event_handler(context, PHPSPY_TRACE_EVENT_STACK_BEGIN);
    while (remote_execute_data && depth != opt_max_stack_depth) { /* TODO make options struct */
        memset(&execute_data, 0, sizeof(execute_data));
        memset(&zfunc, 0, sizeof(zfunc));
        memset(&zstring, 0, sizeof(zstring));
        memset(&zce, 0, sizeof(zce));
        memset(&sapi_globals, 0, sizeof(sapi_globals));

        try_copy_proc_mem("execute_data", remote_execute_data, &execute_data, sizeof(execute_data));
        try_copy_proc_mem("zfunc", execute_data.func, &zfunc, sizeof(zfunc));
        if (zfunc.common.function_name) {
            try(rv, copy_zstring(context, "function_name", zfunc.common.function_name, frame->func, sizeof(frame->func), &frame->func_len));
        } else {
            frame->func_len = snprintf(frame->func, sizeof(frame->func), "<main>");
        }
        if (zfunc.common.scope) {
            try_copy_proc_mem("zce", zfunc.common.scope, &zce, sizeof(zce));
            try(rv, copy_zstring(context, "class_name", zce.name, frame->class, sizeof(frame->class), &frame->class_len));
        } else {
            frame->class[0] = '\0';
            frame->class_len = 0;
        }
        if (zfunc.type == 2) {
            try(rv, copy_zstring(context, "filename", zfunc.op_array.filename, frame->file, sizeof(frame->file), &frame->file_len));
            frame->lineno = zfunc.op_array.line_start;
            /* TODO add comments */
            if (HASH_CNT(hh, varpeek_map) > 0) {
                varpeek_find(context, varpeek_map, &execute_data, remote_execute_data, &zfunc.op_array, frame->file);
            }
        } else {
            frame->file_len = snprintf(frame->file, sizeof(frame->file), "<internal>");
            frame->lineno = -1;
        }
        frame->depth = depth;
        context->event_handler(context, PHPSPY_TRACE_EVENT_FRAME);
        remote_execute_data = execute_data.prev_execute_data;
        depth += 1;
    }
    if (depth < 1) {
        return 1;
    } else if (opt_capture_req) {
        request = &context->event.request;
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
        context->event_handler(context, PHPSPY_TRACE_EVENT_REQUEST);
    }
    context->event_handler(context, PHPSPY_TRACE_EVENT_STACK_END);

    /* TODO feature to print core_globals */
    /* try_copy_proc_mem("core_globals", (void*)target->core_globals_addr, &core_globals, sizeof(core_globals)); */
    /* copy_zarray(context, core_globals.http_globals[3].value.arr) */
    return 0;
}

static int varpeek_find(trace_context_t *context, varpeek_entry_t *varpeek_map, zend_execute_data *local_execute_data, zend_execute_data *remote_execute_data, zend_op_array *op_array, char *file) {
    int rv;
    int i;
    char tmp[PHPSPY_STR_SIZE];
    size_t tmp_len;
    zend_string *zstrp;
    varpeek_entry_t *file_entry, *line_entry, *var_entry;
    zend_op zop;
    zval zv;

    memset(&zop, 0, sizeof(zop));
    try_copy_proc_mem("opline", (void*)local_execute_data->opline, &zop, sizeof(zop));

    HASH_FIND_STR(varpeek_map, file, file_entry);
    if (!file_entry) return 0;
    HASH_FIND_INT(file_entry->map, &zop.lineno, line_entry);
    if (!line_entry) return 0;

    for (i = 0; i < op_array->last_var; i++) {
        try_copy_proc_mem("var", op_array->vars + i, &zstrp, sizeof(zstrp));
        try(rv, copy_zstring(context, "var", zstrp, tmp, sizeof(tmp), &tmp_len));
        tmp[tmp_len] = '\0';
        HASH_FIND_STR(line_entry->map, tmp, var_entry);
        if (!var_entry) continue;
        try_copy_proc_mem("zval", ((zval*)(remote_execute_data)) + ((int)(5 + i)), &zv, sizeof(zv));
        try(rv, copy_zval(context, &zv, tmp, sizeof(tmp), &tmp_len));
        context->event.varpeek.entry = var_entry;
        context->event.varpeek.zval_str = tmp;
        context->event.varpeek.filename = file;
        context->event.varpeek.lineno = zop.lineno;
        try(rv, context->event_handler(context, PHPSPY_TRACE_EVENT_VARPEEK));
    }

    return 0;
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
    return 0;
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
            try(rv, copy_zarray(context, local_zval->value.arr, buf, buf_size, buf_len));
            break;
        default:
            /* TODO handle other zval types */
            /* fprintf(context->fout, "value not supported, found type: %d\n", type); */
            return 1;
    }
    return 0;
}

static int copy_zarray(trace_context_t *context, zend_array *local_arr, char *buf, size_t buf_size, size_t *buf_len) {
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

    *buf_len = (size_t)(buf - obuf);
    return 0;
}

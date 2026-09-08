#define try_copy_proc_mem(__what, __raddr, __laddr, __size) \
    try(rv, copy_proc_mem(context->target.pid, (__what), (__raddr), (__laddr), (__size)))

static int stack_collect(trace_context *context, zend_execute_data *remote_execute_data);
static int stack_emit_frame(trace_context *context, zend_execute_data *remote_execute_data, int depth, zend_execute_data **next_out);
static int trace_stack(trace_context *context, zend_execute_data *remote_execute_data, int *depth);
static int trace_request_info(trace_context *context);
static int trace_memory_info(trace_context *context);
static int trace_globals(trace_context *context);
static int trace_locals(trace_context *context, zend_op *zop, zend_execute_data *remote_execute_data, zend_op_array *op_array, char *file, int file_len);
static int trace_pdo(trace_context *context, zend_execute_data *remote_execute_data, zend_execute_data *local_execute_data, trace_frame *frame);

static int copy_executor_globals(trace_context *context, zend_executor_globals *executor_globals);
static int copy_zarray_bucket(trace_context *context, zend_array *rzarray, const char *key, Bucket *lbucket);

static int sprint_zstring(trace_context *context, const char *what, zend_string *lzstring, char *buf, size_t buf_size, size_t *buf_len);
static int sprint_zval(trace_context *context, zval *lzval, char *buf, size_t buf_size, size_t *buf_len);
static int sprint_zarray(trace_context *context, zend_array *rzarray, char *buf, size_t buf_size, size_t *buf_len);
static int sprint_zarray_val(trace_context *context, zend_array *rzarray, const char *key, char *buf, size_t buf_size, size_t *buf_len);
static int sprint_zarray_bucket(trace_context *context, Bucket *lbucket, char *buf, size_t buf_size, size_t *buf_len);
static int sprint_zarray_packed(trace_context *context, int idx, zval *lzval, char *buf, size_t buf_size, size_t *buf_len);
static int sprint_pdo_binds(trace_context *context, zend_array *rht, char *buf, size_t buf_size, size_t *buf_len);
static int sprint_pdo_bind(trace_context *context, zval *lzval, char *buf, size_t buf_size, size_t *buf_len);

static int should_stop_trace(int rv);

static int do_trace(trace_context *context) {
    int rv, depth;
    zend_executor_globals executor_globals;

    try(rv, copy_executor_globals(context, &executor_globals));
    try(rv, context->event_handler(context, PHPSPY_TRACE_EVENT_STACK_BEGIN));

    rv = PHPSPY_OK;
    rv |= trace_stack(context, executor_globals.current_execute_data, &depth);
    if (should_stop_trace(rv)) goto do_trace_end;
    if (depth < 1) goto do_trace_end;

    if (opt_capture_req) {
        rv |= trace_request_info(context);
        if (should_stop_trace(rv)) goto do_trace_end;
    }

    if (opt_capture_mem) {
        rv |= trace_memory_info(context);
        if (should_stop_trace(rv)) goto do_trace_end;
    }

    if (HASH_CNT(hh, glopeek_map) > 0) {
        rv |= trace_globals(context);
        if (should_stop_trace(rv)) goto do_trace_end;
    }

do_trace_end:
    if (rv == PHPSPY_OK || opt_continue_on_error) {
        try(rv, context->event_handler(context, PHPSPY_TRACE_EVENT_STACK_END));
    }

    return rv;
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
static int stack_collect(trace_context *context, zend_execute_data *remote) {
    int rv;
    zend_execute_data *next;
    utarray_clear(context->stack_ptrs);
    while (remote) {
        if (utarray_len(context->stack_ptrs) >= PHPSPY_MAX_STACK_WALK) {
            log_error("stack_collect: stack walk limit (%d) reached\n", PHPSPY_MAX_STACK_WALK);
            break;
        }
        utarray_push_back(context->stack_ptrs, &remote);
        try_copy_proc_mem(
            "prev_execute_data",
            ((char *)remote) + offsetof(zend_execute_data, prev_execute_data),
            &next, sizeof(next)
        );
        remote = next;
    }
    return PHPSPY_OK;
}

static int stack_emit_frame(trace_context *context, zend_execute_data *remote, int depth, zend_execute_data **next_out) {
    int rv;
    zend_execute_data execute_data;
    zend_function zfunc;
    zend_string zstring;
    zend_class_entry zce;
    zend_op zop;
    trace_target *target;
    trace_frame *frame;

    target = &context->target;
    frame = &context->event.frame;

    memset(&execute_data, 0, sizeof(execute_data));
    memset(&zfunc, 0, sizeof(zfunc));
    memset(&zstring, 0, sizeof(zstring));
    memset(&zce, 0, sizeof(zce));
    memset(&zop, 0, sizeof(zop));

    try_copy_proc_mem("execute_data", remote, &execute_data, sizeof(execute_data));
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
        if (HASH_CNT(hh, varpeek_map) > 0) {
            if (copy_proc_mem(target->pid, "opline", (void*)execute_data.opline, &zop, sizeof(zop)) == PHPSPY_OK) {
                trace_locals(context, &zop, remote, &zfunc.op_array, frame->loc.file, frame->loc.file_len);
            }
        }
    } else {
        frame->loc.file_len = snprintf(frame->loc.file, sizeof(frame->loc.file), "<internal>");
        frame->loc.lineno = -1;
    }
    frame->depth = depth;
    try(rv, context->event_handler(context, PHPSPY_TRACE_EVENT_FRAME));
    if (opt_peek_pdo) {
        trace_pdo(context, remote, &execute_data, frame);
    }
    if (next_out) *next_out = execute_data.prev_execute_data;
    return PHPSPY_OK;
}

static int trace_stack(trace_context *context, zend_execute_data *remote_execute_data, int *depth) {
    int rv, i, total_depth, keep_inner, keep_outer_from, num_elided;
    zend_execute_data **pp;
    trace_frame *frame;

    *depth = 0;

    if (opt_max_stack_depth_from_root >= 0) {
        try(rv, stack_collect(context, remote_execute_data));
        total_depth = utarray_len(context->stack_ptrs);

        keep_inner = opt_max_stack_depth_from_leaf >= 0 ? opt_max_stack_depth_from_leaf : 0;
        keep_outer_from = keep_inner;
        num_elided = 0;
        if (keep_inner + opt_max_stack_depth_from_root < total_depth) {
            keep_outer_from = total_depth - opt_max_stack_depth_from_root;
            num_elided = keep_outer_from - keep_inner;
        }

        frame = &context->event.frame;
        for (i = 0; i < total_depth; i++) {
            if (i == keep_inner && num_elided > 0) {
                frame->loc.func_len = snprintf(frame->loc.func, sizeof(frame->loc.func), "<elided:%d>", num_elided);
                frame->loc.class[0] = '\0';
                frame->loc.class_len = 0;
                frame->loc.file_len = snprintf(frame->loc.file, sizeof(frame->loc.file), "<elided>");
                frame->loc.lineno = -1;
                frame->depth = i;
                try(rv, context->event_handler(context, PHPSPY_TRACE_EVENT_FRAME));
            }
            if (i >= keep_inner && i < keep_outer_from) continue;
            pp = (zend_execute_data **)utarray_eltptr(context->stack_ptrs, (unsigned)i);
            try(rv, stack_emit_frame(context, *pp, i, NULL));
        }
        *depth = total_depth;
    } else {
        while (remote_execute_data && *depth != opt_max_stack_depth_from_leaf) {
            if (*depth >= PHPSPY_MAX_STACK_WALK) {
                log_error("trace_stack: stack walk limit (%d) reached\n", PHPSPY_MAX_STACK_WALK);
                break;
            }
            try(rv, stack_emit_frame(context, remote_execute_data, *depth, &remote_execute_data));
            *depth += 1;
        }
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
static int trace_request_info(trace_context *context) {
    int rv;
    sapi_globals_struct sapi_globals;
    trace_target *target;
    trace_request *request;

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
static int trace_memory_info(trace_context *context) {
    #ifdef USE_ZEND
    (void)context;
    return PHPSPY_ERR; /* zend_alloc_globals is not public */
    #else
    int rv;
    zend_mm_heap mm_heap;
    zend_alloc_globals alloc_globals;
    trace_target *target;

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
static int trace_globals(trace_context *context) {
    int rv;
    glopeek_entry *gentry, *gentry_tmp;
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
            context->event.glopeek.zval_str_len = context->buf_len;
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
static int trace_locals(trace_context *context, zend_op *zop, zend_execute_data *remote_execute_data, zend_op_array *op_array, char *file, int file_len) {
    int rv, i, num_vars_found, num_vars_peeking;
    char tmp[PHPSPY_STR_SIZE];
    size_t tmp_len;
    zend_string *zstrp;
    varpeek_entry *entry;
    varpeek_var *var;
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
        /* See ZEND_CALL_VAR_NUM macro in php-src. The frame-slot count is
           version-dependent (PHP 7.0 differs from every later version); see
           phpspy_frame_slot, defined per-phpv in phpspy_trace_tpl.c and for
           USE_ZEND in structs/structs.h. */
        try_copy_proc_mem("zval", ((zval*)(remote_execute_data)) + ((int)(phpspy_frame_slot + i)), &zv, sizeof(zv));
        try(rv, sprint_zval(context, &zv, tmp, sizeof(tmp), &tmp_len));
        context->event.varpeek.entry = entry;
        context->event.varpeek.var = var;
        context->event.varpeek.zval_str = tmp;
        context->event.varpeek.zval_str_len = tmp_len;
        try(rv, context->event_handler(context, PHPSPY_TRACE_EVENT_VARPEEK));
        if (num_vars_found >= num_vars_peeking) break;
    }

    return PHPSPY_OK;
}

/**
 * If the current frame is PDOStatement::execute, PDO::query, or PDO::exec,
 * emit varpeek events named #pdo_sql (and #pdo_args, when binds are present).
 */
static int trace_pdo(trace_context *context, zend_execute_data *remote_execute_data, zend_execute_data *local_execute_data, trace_frame *frame) {
    int rv, is_stmt_execute, is_pdo_query_or_exec;
    uint32_t num_args;
    varpeek_entry entry;
    varpeek_var var_sql, var_args;
    zend_object lobj;
    pdo_stmt_t lstmt;
    zval first_arg;
    char buf[PHPSPY_STR_SIZE];
    size_t buf_len;
    uint8_t this_type;

    void *robj;

    memset(&entry, 0, sizeof(entry));
    memset(&var_sql, 0, sizeof(var_sql));
    memset(&var_args, 0, sizeof(var_args));

    is_stmt_execute = (strcmp(frame->loc.class, "PDOStatement") == 0
                       && strcmp(frame->loc.func, "execute") == 0);
    is_pdo_query_or_exec = (strcmp(frame->loc.class, "PDO") == 0
                            && (strcmp(frame->loc.func, "query") == 0
                                || strcmp(frame->loc.func, "exec") == 0));
    if (!is_stmt_execute && !is_pdo_query_or_exec) return 0;

    snprintf(entry.filename_lineno, sizeof(entry.filename_lineno),
             "%.96s::%.96s", frame->loc.class, frame->loc.func);
    snprintf(var_sql.name,  sizeof(var_sql.name),  "#pdo_sql");
    snprintf(var_args.name, sizeof(var_args.name), "#pdo_args");

    num_args = local_execute_data->This.u2.next;
    robj = (void*)(uintptr_t)local_execute_data->This.value.lval;

    if (is_stmt_execute) {
        /* PDOStatement::$queryString lives at properties_table[0]. */
        this_type = local_execute_data->This.u1.v.type;
        if (this_type != PHPSPY_ZVAL_TYPE_OBJECT) return 1;
        if (!robj) return 1;

        try_copy_proc_mem("pdo_this", robj, &lobj, sizeof(lobj));

        if (lobj.properties_table[0].u1.v.type == PHPSPY_ZVAL_TYPE_STRING) {
            try(rv, sprint_zstring(context, "pdo_qs",
                lobj.properties_table[0].value.str, buf, sizeof(buf), &buf_len));
            context->event.varpeek.entry = &entry;
            context->event.varpeek.var = &var_sql;
            context->event.varpeek.zval_str = buf;
            context->event.varpeek.zval_str_len = buf_len;
            try(rv, context->event_handler(context, PHPSPY_TRACE_EVENT_VARPEEK));
        }

        if (num_args > 0) {
            /* ->execute(...) */
            try_copy_proc_mem("pdo_arg0",
                ((zval*)remote_execute_data) + 5, &first_arg, sizeof(first_arg));
            if (first_arg.u1.v.type == PHPSPY_ZVAL_TYPE_ARRAY) {
                rv = sprint_zarray(context, first_arg.value.arr, buf, sizeof(buf), &buf_len);
                if (rv == PHPSPY_OK && buf_len > 0) {
                    context->event.varpeek.entry = &entry;
                    context->event.varpeek.var = &var_args;
                    context->event.varpeek.zval_str = buf;
                    context->event.varpeek.zval_str_len = buf_len;
                    try(rv, context->event_handler(context, PHPSPY_TRACE_EVENT_VARPEEK));
                }
            }
        } else {
            /* ->bind... */
            void *rstmt = (void*)((char*)robj - offsetof(pdo_stmt_t, std));
            try_copy_proc_mem("pdo_stmt", rstmt, &lstmt, sizeof(lstmt));
            if (lstmt.bound_params) {
                rv = sprint_pdo_binds(context, lstmt.bound_params, buf, sizeof(buf), &buf_len);
                if (rv == PHPSPY_OK && buf_len > 0) {
                    context->event.varpeek.entry = &entry;
                    context->event.varpeek.var = &var_args;
                    context->event.varpeek.zval_str = buf;
                    context->event.varpeek.zval_str_len = buf_len;
                    try(rv, context->event_handler(context, PHPSPY_TRACE_EVENT_VARPEEK));
                }
            }
        }
    } else {
        /* PDO::query / PDO::exec */
        if (num_args < 1) return 1;
        try_copy_proc_mem("pdo_arg0",
            ((zval*)remote_execute_data) + 5, &first_arg, sizeof(first_arg));
        if (first_arg.u1.v.type == PHPSPY_ZVAL_TYPE_STRING) {
            try(rv, sprint_zstring(context, "pdo_sql",
                first_arg.value.str, buf, sizeof(buf), &buf_len));
            context->event.varpeek.entry = &entry;
            context->event.varpeek.var = &var_sql;
            context->event.varpeek.zval_str = buf;
            context->event.varpeek.zval_str_len = buf_len;
            try(rv, context->event_handler(context, PHPSPY_TRACE_EVENT_VARPEEK));
        }
    }

    return 1;
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
static int copy_executor_globals(trace_context *context, zend_executor_globals *executor_globals) {
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
static int copy_zarray_bucket(trace_context *context, zend_array *rzarray, const char *key, Bucket *lbucket) {
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
static int sprint_zstring(trace_context *context, const char *what, zend_string *rzstring, char *buf, size_t buf_size, size_t *buf_len) {
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
static int sprint_zval(trace_context *context, zval *lzval, char *buf, size_t buf_size, size_t *buf_len) {
    int rv;
    int type;
    type = (int)lzval->u1.v.type;
    switch (type) {
        case PHPSPY_ZVAL_TYPE_LONG:
            snprintf(buf, buf_size, "%ld", lzval->value.lval);
            *buf_len = strlen(buf);
            break;
        case PHPSPY_ZVAL_TYPE_DOUBLE:
            snprintf(buf, buf_size, "%f", lzval->value.dval);
            *buf_len = strlen(buf);
            break;
        case PHPSPY_ZVAL_TYPE_STRING:
            try(rv, sprint_zstring(context, "zval", lzval->value.str, buf, buf_size, buf_len));
            break;
        case PHPSPY_ZVAL_TYPE_ARRAY:
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
static int sprint_zarray(trace_context *context, zend_array *rzarray, char *buf, size_t buf_size, size_t *buf_len) {
    int rv, i, array_len, is_packed;
    size_t tmp_len;
    zend_array lzarray;
    Bucket buckets[PHPSPY_MAX_ARRAY_BUCKETS];
    zval zvals[PHPSPY_MAX_ARRAY_BUCKETS];
    char *obuf = buf;

    try_copy_proc_mem("array", rzarray, &lzarray, sizeof(lzarray));
    array_len = PHPSPY_MIN(lzarray.nNumUsed, PHPSPY_MAX_ARRAY_BUCKETS);
    is_packed = (lzarray.flags & PHPSPY_HASH_FLAG_PACKED) != 0;

    if (is_packed) {
        try_copy_proc_mem("zvals", lzarray.arData, zvals, sizeof(zval) * array_len);
    } else {
        try_copy_proc_mem("buckets", lzarray.arData, buckets, sizeof(Bucket) * array_len);
    }

    for (i = 0; i < array_len; i++) {
        if (is_packed) {
            try(rv, sprint_zarray_packed(context, i, &zvals[i], buf, buf_size, &tmp_len));
        } else {
            try(rv, sprint_zarray_bucket(context, &buckets[i], buf, buf_size, &tmp_len));
        }
        if (tmp_len == 0) continue;
        buf += tmp_len;
        buf_size -= tmp_len;
        if (buf_size < 2) break;
        *buf++ = ',';
        --buf_size;
    }
    if (buf > obuf && *(buf - 1) == ',') --buf;

    *buf_len = (size_t)(buf - obuf);
    return PHPSPY_OK;
}

static int sprint_zarray_packed(trace_context *context, int idx, zval *lzval, char *buf, size_t buf_size, size_t *buf_len) {
    int rv, n;
    size_t tmp_len;
    char *obuf = buf;

    *buf_len = 0;
    if (lzval->u1.v.type == PHPSPY_ZVAL_TYPE_UNDEF) return PHPSPY_OK;

    n = snprintf(buf, buf_size, "%d=", idx);
    if (n < 0 || (size_t)n >= buf_size) return PHPSPY_OK;
    buf += n;
    buf_size -= n;

    try(rv, sprint_zval(context, lzval, buf, buf_size, &tmp_len));
    buf += tmp_len;

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
static int sprint_zarray_val(trace_context *context, zend_array *rzarray, const char *key, char *buf, size_t buf_size, size_t *buf_len) {
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
static int sprint_zarray_bucket(trace_context *context, Bucket *lbucket, char *buf, size_t buf_size, size_t *buf_len) {
    int rv;
    char tmp_key[PHPSPY_STR_SIZE];
    size_t tmp_len;
    char *obuf;

    *buf_len = 0;
    if (lbucket->val.u1.v.type == PHPSPY_ZVAL_TYPE_UNDEF) return PHPSPY_OK;

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

/**
 * Print a pdo_stmt_t->bound_params HashTable as a comma-separated
 * "name=value" list. Buckets store IS_PTR zvals whose value points at a
 * pdo_bound_param_data, not at another zval (because PDO uses
 * zend_hash_*_update_mem to register binds).
 */
static int sprint_pdo_binds(trace_context *context, zend_array *rht, char *buf, size_t buf_size, size_t *buf_len) {
    int rv, i, used, is_packed;
    size_t tmp_len;
    zend_array lht;
    Bucket buckets[PHPSPY_MAX_ARRAY_BUCKETS];
    zval zvals[PHPSPY_MAX_ARRAY_BUCKETS];
    char *obuf = buf;

    *buf_len = 0;
    if (!rht) return PHPSPY_OK;

    try_copy_proc_mem("pdo_binds_ht", rht, &lht, sizeof(lht));
    used = PHPSPY_MIN(lht.nNumUsed, PHPSPY_MAX_ARRAY_BUCKETS);
    if (used <= 0 || !lht.arData) return PHPSPY_OK;

    is_packed = (lht.flags & PHPSPY_HASH_FLAG_PACKED) != 0;
    if (is_packed) {
        try_copy_proc_mem("pdo_binds_zvals", lht.arData, zvals, sizeof(zval) * used);
    } else {
        try_copy_proc_mem("pdo_binds_buckets", lht.arData, buckets, sizeof(Bucket) * used);
    }

    for (i = 0; i < used; i++) {
        zval *zv = is_packed ? &zvals[i] : &buckets[i].val;
        try(rv, sprint_pdo_bind(context, zv, buf, buf_size, &tmp_len));
        if (tmp_len == 0) continue;
        buf += tmp_len;
        buf_size -= tmp_len;
        if (buf_size < 2) break;
        *buf++ = ',';
        --buf_size;
    }
    if (buf > obuf && *(buf - 1) == ',') --buf;

    *buf_len = (size_t)(buf - obuf);
    return PHPSPY_OK;
}

static int sprint_pdo_bind(trace_context *context, zval *lzval, char *buf, size_t buf_size, size_t *buf_len) {
    int rv, n;
    size_t tmp_len, name_len;
    pdo_bound_param_data lbp;
    char tmp_name[PHPSPY_STR_SIZE];
    void *rbp, *rref;
    zval lref;
    zval *vptr;
    char *obuf = buf;

    *buf_len = 0;
    if (lzval->u1.v.type == PHPSPY_ZVAL_TYPE_UNDEF) return PHPSPY_OK;

    rbp = (void *)(uintptr_t)lzval->value.lval;
    if (!rbp) return PHPSPY_OK;
    if (copy_proc_mem(context->target.pid, "pdo_bp", rbp, &lbp, sizeof(lbp)) != PHPSPY_OK) return PHPSPY_OK;

    if (lbp.name) {
        if (sprint_zstring(context, "bp_name", lbp.name, tmp_name, sizeof(tmp_name), &name_len) != PHPSPY_OK) return PHPSPY_OK;
    } else {
        snprintf(tmp_name, sizeof(tmp_name), "%ld", (long)lbp.paramno);
        name_len = strlen(tmp_name);
    }

    n = snprintf(buf, buf_size, "%.*s=", (int)name_len, tmp_name);
    if (n < 0 || (size_t)n >= buf_size) return PHPSPY_OK;
    buf += n;
    buf_size -= n;

    vptr = &lbp.parameter;
    if (vptr->u1.v.type == PHPSPY_ZVAL_TYPE_REFERENCE) {
        rref = (void *)(uintptr_t)vptr->value.lval;
        if (rref && copy_proc_mem(context->target.pid, "pdo_ref", (char *)rref + 8, &lref, sizeof(lref)) == PHPSPY_OK) {
            vptr = &lref;
        }
    }

    try(rv, sprint_zval(context, vptr, buf, buf_size, &tmp_len));
    buf += tmp_len;

    *buf_len = (size_t)(buf - obuf);
    return PHPSPY_OK;
}

#ifndef PHPSPY_TRACE_ONCE
#define PHPSPY_TRACE_ONCE
static int should_stop_trace(int rv) {
    return (rv & PHPSPY_ERR_PID_DEAD) != 0
        || (rv & PHPSPY_ERR_BUF_FULL) != 0
        || (rv != PHPSPY_OK && !opt_continue_on_error);
}
#endif

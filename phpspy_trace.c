#define try_copy_proc_mem(__what, __raddr, __laddr, __size) \
    try(rv, copy_proc_mem(context, (__what), (__raddr), (__laddr), (__size)))

static int varpeek_find(trace_context_t *context, varpeek_entry_t *varpeek_map, zend_execute_data *local_execute_data, zend_execute_data *remote_execute_data, zend_op_array *op_array, char *file, int file_len);
static int varpeek_copy_zstring(trace_context_t *context, zend_string *string_ptr, char **retstr, size_t *retlen);
static int varpeek_print_zval(trace_context_t *context, zval *local_zval);
static int varpeek_print_array(trace_context_t *context, zend_array *local_arr);

static int dump_trace(trace_context_t *context) { //pid_t pid, FILE *fout, uint64_t executor_globals_addr, uint64_t sapi_globals_addr, uint64_t core_globals_addr) {
    // TODO replace context with struct ptr
    char func[PHPSPY_STR_LEN+1];
    char file[PHPSPY_STR_LEN+1];
    char class[PHPSPY_STR_LEN+1];
    char uri[PHPSPY_STR_LEN+1];
    char path[PHPSPY_STR_LEN+1];
    char qstring[PHPSPY_STR_LEN+1];
    char cookie[PHPSPY_STR_LEN+1];
    int file_len;
    int func_len;
    int class_len;
    int lineno;
    int depth;
    int wrote_trace;
    int rv;
    zend_execute_data *remote_execute_data;
    zend_executor_globals executor_globals;
    zend_execute_data execute_data;
    zend_class_entry zce;
    zend_function zfunc;
    zend_string zstring;
    sapi_globals_struct sapi_globals;

    depth = 0;
    wrote_trace = 0;

    executor_globals.current_execute_data = NULL;
    try_copy_proc_mem("executor_globals", (void*)context->executor_globals_addr, &executor_globals, sizeof(executor_globals));
    remote_execute_data = executor_globals.current_execute_data;

    // TODO reduce number of copy calls
    while (remote_execute_data && depth != opt_max_stack_depth) {
        memset(&execute_data, 0, sizeof(execute_data));
        memset(&zfunc, 0, sizeof(zfunc));
        memset(&zstring, 0, sizeof(zstring));
        memset(&zce, 0, sizeof(zce));
        memset(&sapi_globals, 0, sizeof(sapi_globals));

        try_copy_proc_mem("execute_data", remote_execute_data, &execute_data, sizeof(execute_data));
        try_copy_proc_mem("zfunc", execute_data.func, &zfunc, sizeof(zfunc));
        if (zfunc.common.function_name) {
            // TODO DRY with varpeek_copy_zstring
            try_copy_proc_mem("function_name", zfunc.common.function_name, &zstring, sizeof(zstring));
            func_len = PHPSPY_MIN(zstring.len, PHPSPY_STR_LEN);
            try_copy_proc_mem("function_name.val", ((char*)zfunc.common.function_name) + zend_string_val_offset, func, func_len);
        } else {
            func_len = snprintf(func, sizeof(func), "<main>");
        }
        if (zfunc.common.scope) {
            try_copy_proc_mem("zce", zfunc.common.scope, &zce, sizeof(zce));
            try_copy_proc_mem("class_name", zce.name, &zstring, sizeof(zstring));
            class_len = PHPSPY_MIN(zstring.len, PHPSPY_STR_LEN);
            try_copy_proc_mem("class_name.val", ((char*)zce.name) + zend_string_val_offset, class, class_len);
            if (class_len+2 <= PHPSPY_STR_LEN) {
                class[class_len+0] = ':';
                class[class_len+1] = ':';
                class[class_len+2] = '\0';
                class_len += 2;
            }
        } else {
            class[0] = '\0';
            class_len = 0;
        }
        if (zfunc.type == 2) {
            try_copy_proc_mem("filename", zfunc.op_array.filename, &zstring, sizeof(zstring));
            file_len = PHPSPY_MIN(zstring.len, PHPSPY_STR_LEN);
            try_copy_proc_mem("filename.val", ((char*)zfunc.op_array.filename) + zend_string_val_offset, file, file_len);
            lineno = zfunc.op_array.line_start;
            // TODO split this function up, add comments
            if (HASH_CNT(hh, varpeek_map) > 0) {
                varpeek_find(context, varpeek_map, &execute_data, remote_execute_data, &zfunc.op_array, file, file_len);
            }
        } else {
            file_len = snprintf(file, sizeof(file), "<internal>");
            lineno = -1;
        }
        fprintf(context->fout, "%d %.*s%.*s %.*s:%d%s", depth, class_len, class, func_len, func, file_len, file, lineno, opt_frame_delim);
        if (!wrote_trace) wrote_trace = 1;
        remote_execute_data = execute_data.prev_execute_data;
        depth += 1;

    }
    if (wrote_trace) {
        if (opt_capture_req) {
            // TODO review opt_capture_req*
            try_copy_proc_mem("sapi_globals", (void*)context->sapi_globals_addr, &sapi_globals, sizeof(sapi_globals));
            // TODO try(func) instead of macro
            #define try_copy_sapi_global_field(__field, __local) do {                                       \
                if ((opt_capture_req_ ## __local) && sapi_globals.request_info.__field) {                   \
                    try_copy_proc_mem(#__field, sapi_globals.request_info.__field, &__local, PHPSPY_STR_LEN+1);    \
                } else {                                                                                    \
                    __local[0] = '-';                                                                       \
                    __local[1] = '\0';                                                                      \
                }                                                                                           \
            } while (0)
            try_copy_sapi_global_field(query_string, qstring);
            try_copy_sapi_global_field(cookie_data, cookie);
            try_copy_sapi_global_field(request_uri, uri);
            try_copy_sapi_global_field(path_translated, path);
            #undef try_copy_sapi_global_field
            fprintf(context->fout, "# %f %s %s %s %s%s", sapi_globals.global_request_time, uri, qstring, path, cookie, opt_trace_delim);
        } else {
            // TODO review output format(s)
            fprintf(context->fout, "# - - - - -%s", opt_trace_delim);
        }
    }

    // TODO feature to print core_globals
    // try_copy_proc_mem("core_globals", (void*)context->core_globals_addr, &core_globals, sizeof(core_globals));
    // varpeek_print_array(context, core_globals.http_globals[3].value.arr)
    return 0;
}

static int varpeek_find(trace_context_t *context, varpeek_entry_t *varpeek_map, zend_execute_data *local_execute_data, zend_execute_data *remote_execute_data, zend_op_array *op_array, char *file, int file_len) {
    int rv;
    int i;
    char *tmp;
    size_t tmp_len;
    zend_string *zstrp;
    varpeek_entry_t *varpeek;
    char varpeek_key[PHPSPY_VARPEEK_KEY_SIZE];
    zend_op zop;
    zval zv;

    memset(&zop, 0, sizeof(zop));
    try_copy_proc_mem("opline", (void*)local_execute_data->opline, &zop, sizeof(zop));

    snprintf(varpeek_key, sizeof(varpeek_key), "%.*s:%d", file_len, file, zop.lineno);
    HASH_FIND_STR(varpeek_map, varpeek_key, varpeek);
    if (!varpeek) return 0;

    for (i = 0; i < op_array->last_var; i++) {
        try_copy_proc_mem("var", op_array->vars + i, &zstrp, sizeof(zstrp));
        try(rv, varpeek_copy_zstring(context, zstrp, &tmp, &tmp_len));
        if (strncmp(tmp, varpeek->varname, tmp_len) != 0) continue;
        try_copy_proc_mem("zval", ((zval*)(remote_execute_data)) + ((int)(5 + i)), &zv, sizeof(zv));
        // TODO review output formats
        fprintf(context->fout, "# varpeek %s@%s = ", varpeek->varname, varpeek->filename_lineno);
        try(rv, varpeek_print_zval(context, &zv));
        return 0;
    }

    return 0;
}

static int varpeek_copy_zstring(trace_context_t *context, zend_string *string_ptr, char **retstr, size_t *retlen) {
    int rv;
    zend_string string_partial;
    zend_string *string_full;
    size_t copy_size;

    string_full = (zend_string*)context->buf;
    try_copy_proc_mem("string_partial", string_ptr, &string_partial, sizeof(string_partial));

    // TODO can avoid recopying the first half of the struct
    // TODO DRY with string copying in dump_trace
    copy_size = PHPSPY_MIN(sizeof(context->buf)-1, sizeof(string_partial) + string_partial.len);
    try_copy_proc_mem("string_full", string_ptr, string_full, copy_size);

    *retstr = context->buf + sizeof(string_partial) - 1;
    *retlen = copy_size - sizeof(string_partial);
    return 0;
}

static int varpeek_print_zval(trace_context_t *context, zval *local_zval) {
    int rv;
    int type;
    char *tmp;
    size_t tmp_len;
    type = (int)local_zval->u1.v.type;
    switch (type) {
        case IS_LONG:
            fprintf(context->fout, "%ld\n", local_zval->value.lval);
            break;
        case IS_DOUBLE:
            fprintf(context->fout, "%lf\n", local_zval->value.dval);
            break;
        case IS_STRING:
            try(rv, varpeek_copy_zstring(context, local_zval->value.str, &tmp, &tmp_len));
            fprintf(context->fout, "\"%.*s\"\n", (int)tmp_len, tmp);
            break;
        case IS_ARRAY:
            try(rv, varpeek_print_array(context, local_zval->value.arr));
            break;
        default:
            // TODO handle other zval types
            // fprintf(context->fout, "value not supported, found type: %d\n", type);
            break;
    }
    return 0;
}

static int varpeek_print_array(trace_context_t *context, zend_array *local_arr) {
    int rv;
    int i;
    char *tmp;
    size_t tmp_len;
    int length;
    Bucket *buckets;
    zend_array array;

    try_copy_proc_mem("array", local_arr, &array, sizeof(array));
    length = array.nNumOfElements;

    buckets = (Bucket*)malloc(sizeof(Bucket) * length); // TODO potentially leaks, use stack or global
    try_copy_proc_mem("buckets", array.arData, buckets, sizeof(Bucket) * length);

    for (i = 0; i < length; i++) {
        // if it is a non-associative array, don't attempt to print the key
        if (buckets[i].key != 0) {
            try(rv, varpeek_copy_zstring(context, buckets[i].key, &tmp, &tmp_len));
            fprintf(context->fout, "%.*s : ", (int)tmp_len, tmp);
        }
        try(rv, varpeek_print_zval(context, &buckets[i].val));
    }
    free(buckets);
    return 0;
}

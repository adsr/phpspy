#ifdef USE_ZEND
#ifdef snprintf
#undef snprintf
#endif
#endif

static int dump_trace(pid_t pid, FILE *fout, unsigned long long executor_globals_addr, unsigned long long sapi_globals_addr) {
    char func[STR_LEN+1];
    char file[STR_LEN+1];
    char class[STR_LEN+1];
    char uri[STR_LEN+1];
    char path[STR_LEN+1];
    char qstring[STR_LEN+1];
    char cookie[STR_LEN+1];
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
    zend_op zop;
    zend_class_entry zce;
    zend_function zfunc;
    zend_string zstring;
    sapi_globals_struct sapi_globals;

    depth = 0;
    wrote_trace = 0;

    #define try_copy_proc_mem(__what, __raddr, __laddr, __size) do {          \
        if ((rv = copy_proc_mem(pid, (__raddr), (__laddr), (__size))) != 0) { \
            fprintf(stderr, "dump_trace: Failed to copy %s\n", (__what));     \
            if (wrote_trace) printf("%s", opt_trace_delim);                   \
            return rv;                                                        \
        }                                                                     \
    } while(0)

    executor_globals.current_execute_data = NULL;
    try_copy_proc_mem("executor_globals", (void*)executor_globals_addr, &executor_globals, sizeof(executor_globals));
    remote_execute_data = executor_globals.current_execute_data;

    while (remote_execute_data && depth != opt_max_stack_depth) {
        memset(&execute_data, 0, sizeof(execute_data));
        memset(&zfunc, 0, sizeof(zfunc));
        memset(&zstring, 0, sizeof(zstring));
        memset(&zce, 0, sizeof(zce));
        memset(&zop, 0, sizeof(zop));
        memset(&sapi_globals, 0, sizeof(sapi_globals));

        try_copy_proc_mem("execute_data", remote_execute_data, &execute_data, sizeof(execute_data));
        try_copy_proc_mem("zfunc", execute_data.func, &zfunc, sizeof(zfunc));
        if (zfunc.common.function_name) {
            try_copy_proc_mem("function_name", zfunc.common.function_name, &zstring, sizeof(zstring));
            func_len = PHPSPY_MIN(zstring.len, STR_LEN);
            try_copy_proc_mem("function_name.val", ((char*)zfunc.common.function_name) + zend_string_val_offset, func, func_len);
        } else {
            func_len = snprintf(func, sizeof(func), "<main>");
        }
        if (zfunc.common.scope) {
            try_copy_proc_mem("zce", zfunc.common.scope, &zce, sizeof(zce));
            try_copy_proc_mem("class_name", zce.name, &zstring, sizeof(zstring));
            class_len = PHPSPY_MIN(zstring.len, STR_LEN);
            try_copy_proc_mem("class_name.val", ((char*)zce.name) + zend_string_val_offset, class, class_len);
            if (class_len+2 <= STR_LEN) {
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
            try_copy_proc_mem("zop", (void*)execute_data.opline, &zop, sizeof(zop));
            try_copy_proc_mem("filename", zfunc.op_array.filename, &zstring, sizeof(zstring));
            file_len = PHPSPY_MIN(zstring.len, STR_LEN);
            try_copy_proc_mem("filename.val", ((char*)zfunc.op_array.filename) + zend_string_val_offset, file, file_len);
            lineno = zop.lineno;
        } else {
            file_len = snprintf(file, sizeof(file), "<internal>");
            lineno = -1;
        }
        fprintf(fout, "%d %.*s%.*s %.*s:%d%s", depth, class_len, class, func_len, func, file_len, file, lineno, opt_frame_delim);
        if (!wrote_trace) wrote_trace = 1;
        remote_execute_data = execute_data.prev_execute_data;
        depth += 1;
    }
    if (wrote_trace) {
        if (opt_capture_req) {
            try_copy_proc_mem("sapi_globals", (void*)sapi_globals_addr, &sapi_globals, sizeof(sapi_globals));
            #define try_copy_sapi_global_field(__field, __local) do {                                       \
                if ((opt_capture_req_ ## __local) && sapi_globals.request_info.__field) {                   \
                    try_copy_proc_mem(#__field, sapi_globals.request_info.__field, &__local, STR_LEN+1);    \
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
            fprintf(fout, "# %f %s %s %s %s%s", sapi_globals.global_request_time, uri, qstring, path, cookie, opt_trace_delim);
        } else {
            fprintf(fout, "# - - - - -%s", opt_trace_delim);
        }
    }

    #undef try_copy_proc_mem
    return 0;
}

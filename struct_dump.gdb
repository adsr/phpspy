macro define offsetof(st, f) ((size_t)&(((st *)0)->f))
printf "zend_executor_globals size=%lu\n", sizeof(zend_executor_globals)
printf "  .current_execute_data +%lu\n", offsetof(zend_executor_globals, current_execute_data)
printf "\n"
printf "zend_execute_data size=%lu\n", sizeof(zend_execute_data)
printf "  .opline +%lu\n", offsetof(zend_execute_data, opline)
printf "  .func +%lu\n", offsetof(zend_execute_data, func)
printf "  .prev_execute_data +%lu\n", offsetof(zend_execute_data, prev_execute_data)
printf "\n"
printf "zend_function size=%lu\n", sizeof(zend_function)
printf "  .type +%lu\n", offsetof(zend_function, type)
printf "  .common.function_name +%lu\n", offsetof(zend_function, common.function_name)
printf "  .common.scope +%lu\n", offsetof(zend_function, common.scope)
printf "  .op_array.filename +%lu\n", offsetof(zend_function, op_array.filename)
printf "\n"
printf "zend_class_entry size=%lu\n", sizeof(zend_class_entry)
printf "  .name +%lu\n", offsetof(zend_class_entry, name)
printf "\n"
printf "zend_string size=%lu\n", sizeof(zend_string)
printf "  .len +%lu\n", offsetof(zend_string, len)
printf "  .val +%lu\n", offsetof(zend_string, val)
printf "\n"
printf "zend_op size=%lu\n", sizeof(zend_op)
printf "  .lineno +%lu\n", offsetof(zend_op, lineno)
printf "\n"
printf "sapi_request_info=%lu\n", sizeof(sapi_request_info)
printf "  .query_string +%lu\n", offsetof(sapi_request_info, query_string)
printf "  .cookie_data +%lu\n", offsetof(sapi_request_info, cookie_data)
printf "  .path_translated +%lu\n", offsetof(sapi_request_info, path_translated)
printf "  .request_uri +%lu\n", offsetof(sapi_request_info, request_uri)
printf "\n"
printf "sapi_globals_struct=%lu\n", sizeof(sapi_globals_struct)
printf "  .request_info +%lu\n", offsetof(sapi_globals_struct, request_info)
printf "  .global_request_time +%lu\n", offsetof(sapi_globals_struct, global_request_time)
printf "\n"
printf "\n"

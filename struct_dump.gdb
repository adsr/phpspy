macro define offsetof(st, f) ((size_t)&(((st *)0)->f))
macro define typeof(st, f) ((st *)0).f
define fieldof
  printf "  "
  whatis typeof($arg0, $arg1)
  printf "  $arg1 %lu +%lu\n", offsetof($arg0, $arg1), sizeof(typeof($arg0, $arg1))
end

printf "zend_executor_globals\n"
whatis zend_executor_globals
       fieldof zend_executor_globals current_execute_data
printf "\n"

printf "zend_execute_data\n"
whatis zend_execute_data
       fieldof zend_execute_data opline
       fieldof zend_execute_data func
       fieldof zend_execute_data prev_execute_data
       fieldof zend_execute_data symbol_table
printf "\n"

printf "zend_op_array\n"
whatis zend_op_array
       fieldof zend_op_array last_var
       fieldof zend_op_array vars
       fieldof zend_op_array filename
       fieldof zend_op_array line_start
printf "\n"

printf "zend_function\n"
whatis zend_function
       fieldof zend_function type
       fieldof zend_function common
       fieldof zend_function common.function_name
       fieldof zend_function common.scope
       fieldof zend_function op_array
printf "\n"

printf "zend_class_entry\n"
whatis zend_class_entry
       fieldof zend_class_entry name
printf "\n"

printf "zend_string\n"
whatis zend_string
       fieldof zend_string len
       fieldof zend_string val
printf "\n"

printf "zend_op\n"
whatis zend_op
       fieldof zend_op lineno
printf "\n"

printf "sapi_request_info\n"
whatis sapi_request_info
       fieldof sapi_request_info query_string
       fieldof sapi_request_info cookie_data
       fieldof sapi_request_info path_translated
       fieldof sapi_request_info request_uri
printf "\n"

printf "sapi_globals_struct\n"
whatis sapi_globals_struct
       fieldof sapi_globals_struct request_info
       fieldof sapi_globals_struct global_request_time
printf "\n"

printf "php_core_globals\n"
whatis php_core_globals
       fieldof php_core_globals http_globals
printf "\n"

printf "zval\n"
whatis zval
       fieldof zval value
       fieldof zval u1
       fieldof zval u1.v
       fieldof zval u1.v.type
printf "\n"

printf "zend_value\n"
whatis zend_value
       fieldof zend_value lval
       fieldof zend_value dval
       fieldof zend_value str
       fieldof zend_value arr
printf "\n"

printf "zend_array\n"
whatis zend_array
       fieldof zend_array arData
       fieldof zend_array nNumUsed
       fieldof zend_array nNumOfElements
       fieldof zend_array nTableSize
printf "\n"

printf "Bucket\n"
whatis Bucket
       fieldof Bucket val
       fieldof Bucket h
       fieldof Bucket key
printf "\n"

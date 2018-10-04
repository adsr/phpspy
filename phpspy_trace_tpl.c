#define concat1(a, b) a ## b
#define concat2(a, b) concat1(a, b)

#define Bucket                concat2(Bucket_,                phpv)
#define zend_array            concat2(zend_array_,            phpv)
#define zend_executor_globals concat2(zend_executor_globals_, phpv)
#define zend_execute_data     concat2(zend_execute_data_,     phpv)
#define zend_function         concat2(zend_function_,         phpv)
#define zend_class_entry      concat2(zend_class_entry_,      phpv)
#define zend_string           concat2(zend_string_,           phpv)
#define zend_op_array         concat2(zend_op_array_,         phpv)
#define zend_op               concat2(zend_op_,               phpv)
#define sapi_globals_struct   concat2(sapi_globals_struct_,   phpv)
#define sapi_request_info     concat2(sapi_request_info_,     phpv)
#define php_core_globals      concat2(php_core_globals_,      phpv)
#define zval                  concat2(zval_,                  phpv)

#define dump_trace            concat2(dump_trace_,            phpv)
#define copy_zstring          concat2(copy_zstring_,          phpv)
#define varpeek_find          concat2(varpeek_find_,          phpv)
#define varpeek_print_zval    concat2(varpeek_print_zval_,    phpv)
#define varpeek_print_array   concat2(varpeek_print_array_,   phpv)

#include "phpspy_trace.c"

#undef Bucket
#undef zend_array
#undef zend_executor_globals
#undef zend_execute_data
#undef zend_function
#undef zend_class_entry
#undef zend_string
#undef zend_op_array
#undef zend_op
#undef sapi_globals_struct
#undef sapi_request_info
#undef php_core_globals
#undef zval

#undef dump_trace
#undef copy_zstring
#undef varpeek_find
#undef varpeek_print_zval
#undef varpeek_print_array

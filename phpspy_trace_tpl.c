#define concat1(a, b) a ## b
#define concat2(a, b) concat1(a, b)

#define Bucket                concat2(Bucket_,                phpv)
#define php_core_globals      concat2(php_core_globals_,      phpv)
#define sapi_globals_struct   concat2(sapi_globals_struct_,   phpv)
#define sapi_request_info     concat2(sapi_request_info_,     phpv)
#define zend_alloc_globals    concat2(zend_alloc_globals_,    phpv)
#define zend_array            concat2(zend_array_,            phpv)
#define zend_class_entry      concat2(zend_class_entry_,      phpv)
#define zend_execute_data     concat2(zend_execute_data_,     phpv)
#define zend_executor_globals concat2(zend_executor_globals_, phpv)
#define zend_function         concat2(zend_function_,         phpv)
#define zend_mm_heap          concat2(zend_mm_heap_,          phpv)
#define zend_op               concat2(zend_op_,               phpv)
#define zend_op_array         concat2(zend_op_array_,         phpv)
#define zend_string           concat2(zend_string_,           phpv)
#define zval                  concat2(zval_,                  phpv)

#define do_trace              concat2(do_trace_,              phpv)
#define trace_stack           concat2(trace_stack_,           phpv)
#define trace_request_info    concat2(trace_request_info_,    phpv)
#define trace_memory_info     concat2(trace_memory_info_,     phpv)
#define trace_globals         concat2(trace_globals_,         phpv)
#define trace_locals          concat2(trace_locals_,          phpv)
#define copy_executor_globals concat2(copy_executor_globals_, phpv)
#define copy_zarray_bucket    concat2(copy_zarray_bucket_,    phpv)
#define sprint_zstring        concat2(sprint_zstring_,        phpv)
#define sprint_zval           concat2(sprint_zval_,           phpv)
#define sprint_zarray         concat2(sprint_zarray_,         phpv)
#define sprint_zarray_val     concat2(sprint_zarray_val,      phpv)
#define sprint_zarray_bucket  concat2(sprint_zarray_bucket_,  phpv)

#include "phpspy_trace.c"

#undef concat1
#undef concat2

#undef Bucket
#undef php_core_globals
#undef sapi_globals_struct
#undef sapi_request_info
#undef zend_alloc_globals
#undef zend_array
#undef zend_class_entry
#undef zend_execute_data
#undef zend_executor_globals
#undef zend_function
#undef zend_mm_heap
#undef zend_op
#undef zend_op_array
#undef zend_string
#undef zval

#undef do_trace
#undef trace_stack
#undef trace_request_info
#undef trace_memory_info
#undef trace_globals
#undef trace_locals
#undef copy_executor_globals
#undef copy_zarray_bucket
#undef sprint_zstring
#undef sprint_zval
#undef sprint_zarray
#undef sprint_zarray_val
#undef sprint_zarray_bucket

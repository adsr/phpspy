#define concat1(a, b) a ## b
#define concat2(a, b) concat1(a, b)

#define Bucket                concat2(Bucket_,                phpv)
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
#define zend_object           concat2(zend_object_,           phpv)
#define pdo_stmt_t            concat2(pdo_stmt_t_,            phpv)
#define pdo_bound_param_data  concat2(pdo_bound_param_data_,  phpv)

#define do_trace              concat2(do_trace_,              phpv)
#define stack_collect         concat2(stack_collect_,         phpv)
#define stack_emit_frame      concat2(stack_emit_frame_,      phpv)
#define trace_stack           concat2(trace_stack_,           phpv)
#define trace_request_info    concat2(trace_request_info_,    phpv)
#define trace_memory_info     concat2(trace_memory_info_,     phpv)
#define trace_globals         concat2(trace_globals_,         phpv)
#define trace_locals          concat2(trace_locals_,          phpv)
#define trace_pdo             concat2(trace_pdo_,             phpv)
#define copy_executor_globals concat2(copy_executor_globals_, phpv)
#define copy_zarray_bucket    concat2(copy_zarray_bucket_,    phpv)
#define sprint_zstring        concat2(sprint_zstring_,        phpv)
#define sprint_zval           concat2(sprint_zval_,           phpv)
#define sprint_zarray         concat2(sprint_zarray_,         phpv)
#define sprint_zarray_val     concat2(sprint_zarray_val,      phpv)
#define sprint_zarray_bucket  concat2(sprint_zarray_bucket_,  phpv)
#define sprint_zarray_packed  concat2(sprint_zarray_packed_,  phpv)
#define sprint_pdo_binds      concat2(sprint_pdo_binds_,      phpv)
#define sprint_pdo_bind       concat2(sprint_pdo_bind_,       phpv)

/* ZEND_CALL_FRAME_SLOT = ceil(sizeof(zend_execute_data) / sizeof(zval)). Measured
   directly against every supported PHP version's real headers: 6 on 7.0 (which
   still carries execute_data.called_scope, removed in 7.1), 5 on every later
   version (sizeof(zend_execute_data) is 72 or 80 there, both of which round up
   to 5 slots of 16 bytes). */
#if phpv == 70
#define phpspy_frame_slot 6
#else
#define phpspy_frame_slot 5
#endif

#include "phpspy_trace.c"

#undef concat1
#undef concat2

#undef Bucket
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
#undef zend_object
#undef pdo_stmt_t
#undef pdo_bound_param_data

#undef do_trace
#undef stack_collect
#undef stack_emit_frame
#undef trace_stack
#undef trace_request_info
#undef trace_memory_info
#undef trace_globals
#undef trace_locals
#undef trace_pdo
#undef sprint_pdo_binds
#undef sprint_pdo_bind
#undef phpspy_frame_slot
#undef copy_executor_globals
#undef copy_zarray_bucket
#undef sprint_zstring
#undef sprint_zval
#undef sprint_zarray
#undef sprint_zarray_val
#undef sprint_zarray_bucket
#undef sprint_zarray_packed

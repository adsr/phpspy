#define concat1(a, b)         a ## b
#define concat2(a, b)         concat1(a, b)
#define dump_trace            concat2(dump_trace_, phpv)
#define zend_executor_globals concat2(zend_executor_globals_, phpv)
#define zend_execute_data     concat2(zend_execute_data_, phpv)
#define zend_function         concat2(zend_function_, phpv)
#define zend_class_entry      concat2(zend_class_entry_, phpv)
#define zend_string           concat2(zend_string_, phpv)
#define zend_op               concat2(zend_op_, phpv)
#define sapi_globals_struct   concat2(sapi_globals_struct_, phpv)
#define sapi_request_info     concat2(sapi_request_info_, phpv)
#include "phpspy_trace.c"
#undef dump_trace
#undef zend_executor_globals
#undef zend_execute_data
#undef zend_function
#undef zend_class_entry
#undef zend_string
#undef zend_op
#undef sapi_globals_struct
#undef sapi_request_info
#undef concat1
#undef concat2

#define concat1(a, b)         a ## b
#define concat2(a, b)         concat1(a, b)
#define Bucket                concat2(Bucket_, phpv)
#define dump_trace            concat2(dump_trace_, phpv)
#define print_array_recursive concat2(print_array_recursive_, phpv)
#define copy_zend_string      concat2(copy_zend_string_, phpv)
#define zend_array            concat2(zend_array_, phpv)
#define zend_executor_globals concat2(zend_executor_globals_, phpv)
#define zend_execute_data     concat2(zend_execute_data_, phpv)
#define zend_function         concat2(zend_function_, phpv)
#define zend_class_entry      concat2(zend_class_entry_, phpv)
#define zend_string           concat2(zend_string_, phpv)
#define zend_op               concat2(zend_op_, phpv)
#define sapi_globals_struct   concat2(sapi_globals_struct_, phpv)
#define sapi_request_info     concat2(sapi_request_info_, phpv)
#define php_core_globals      concat2(php_core_globals_, phpv)
#define zval                  concat2(zval_, phpv)
#include "phpspy_trace.c"
#undef Bucket
#undef dump_trace
#undef print_array_recursive
#undef copy_zend_string
#undef zend_array
#undef zend_executor_globals
#undef zend_execute_data
#undef zend_function
#undef zend_class_entry
#undef zend_string
#undef zend_op
#undef zval
#undef sapi_globals_struct
#undef sapi_request_info
#undef php_core_globals
#undef concat1
#undef concat2

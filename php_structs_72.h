#ifndef __php_structs_72_h
#define __php_structs_72_h

#include <stdint.h>

typedef struct _zend_executor_globals_72 zend_executor_globals_72;
typedef struct _zend_execute_data_72     zend_execute_data_72;
typedef union  _zend_function_72         zend_function_72;
typedef struct _zend_class_entry_72      zend_class_entry_72;
typedef struct _zend_string_72           zend_string_72;
typedef struct _zend_op_72               zend_op_72;
typedef struct _sapi_globals_struct_72   sapi_globals_struct_72;
typedef struct _sapi_request_info_72     sapi_request_info_72;

// Assumes 8-byte pointers

                                                // offset   length
                                                //
struct __attribute__((__packed__)) _zend_executor_globals_72 {
    uint8_t pad0[480];                          // 0        +480
    zend_execute_data_72 *current_execute_data; // 480      +8
};

struct __attribute__((__packed__)) _zend_execute_data_72 {
    zend_op_72 *opline;                         // 0        +8
    uint8_t pad0[16];                           // 8        +16
    zend_function_72 *func;                     // 24       +8
    uint8_t pad1[16];                           // 32       +16
    zend_execute_data_72 *prev_execute_data;    // 48       +8
};

union __attribute__((__packed__)) _zend_function_72 {
    uint8_t type;                               // 0        +8
    struct {
        uint8_t pad0[8];                        // 0        +8
        zend_string_72 *function_name;          // 8        +8
        zend_class_entry_72 *scope;             // 16       +8
    } common;
    struct {
        uint8_t pad1[120];                      // 0        +120
        zend_string_72 *filename;               // 120      +8
    } op_array;
};

struct __attribute__((__packed__)) _zend_class_entry_72 {
    uint8_t pad0[8];                            // 0        +8
    zend_function_72 *name;                     // 8        +8
};

struct __attribute__((__packed__)) _zend_string_72 {
    uint8_t pad0[16];                           // 0        +16
    size_t len;                                 // 16       +8
    char *val;                                  // 24       +8
};

struct __attribute__((__packed__)) _zend_op_72 {
    uint8_t pad0[24];                           // 0        +24
    uint32_t lineno;                            // 24       +4
};

struct __attribute__((__packed__)) _sapi_request_info_72 {
    uint8_t pad0[32];                           // 0        +32
    char *path_translated;                      // 32       +8
    char *request_uri;                          // 40       +8
};

struct __attribute__((__packed__)) _sapi_globals_struct_72 {
    uint8_t pad0[8];                            // 0        +8
    sapi_request_info_72 request_info;          // 8        +48
    uint8_t pad1[384];                          // 56       +384
    double global_request_time;                 // 440      +8
};

#endif

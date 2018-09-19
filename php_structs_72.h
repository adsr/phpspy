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
typedef struct _php_core_globals_72      php_core_globals_72;
typedef struct _zend_array_72            zend_array_72;
typedef struct _zval_72                  zval_72;
typedef struct _Bucket_72                Bucket_72;

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
    char val[1];                                // 24       +8
};

struct __attribute__((__packed__)) _zend_op_72 {
    uint8_t pad0[24];                           // 0        +24
    uint32_t lineno;                            // 24       +4
};

struct __attribute__((__packed__)) _sapi_request_info_72 {
    uint8_t pad0[8];                            // 0        +8
    char *query_string;                         // 8        +8
    char *cookie_data;                          // 16       +8
    uint8_t pad1[8];                            // 24       +8
    char *path_translated;                      // 32       +8
    char *request_uri;                          // 40       +8
};

struct __attribute__((__packed__)) _sapi_globals_struct_72 {
    uint8_t pad0[8];                            // 0        +8
    sapi_request_info_72 request_info;          // 8        +48
    uint8_t pad1[384];                          // 56       +384
    double global_request_time;                 // 440      +8
};

struct __attribute__((__packed__)) _zval_72 {
    union {
        int64_t lval;
        double dval;
        zend_string_72 *str;
        zend_array_72 *arr;
        // @todo support more zval types
    } value;                                    // 0        +8
    union {
        struct {
            // if big endian gets supported, this order will need to change
            unsigned char type;                 // 8        +1
        } v;
    } u1;
    uint8_t pad0[3];                            // 9        +3
    uint32_t u2;                                // 12       +4
};

struct __attribute__((__packed__)) _php_core_globals_72 {
    uint8_t pad0[368];                          // 0        +368
    zval_72 http_globals[6];                    // 368      +48
};

struct __attribute__((__packed__)) _Bucket_72 {
    zval_72 val;                               // 0        +16
    uint64_t h;                                // 16       +8
    zend_string_72 *key;                       // 24       +32
};

struct __attribute__((__packed__)) _zend_array_72 {
    uint8_t pad0[16];                           // 0        +16
    Bucket_72 *arData;                          // 16       +8
    uint32_t nNumUsed;                          // 24       +4
    uint32_t nNumOfElements;                    // 28       +4
    uint32_t nTableSize;                        // 32       +4
};

#endif

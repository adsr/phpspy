#ifndef __php_structs_73_h
#define __php_structs_73_h

#include <stdint.h>

typedef struct _zend_executor_globals_73 zend_executor_globals_73;
typedef struct _zend_execute_data_73     zend_execute_data_73;
typedef union  _zend_function_73         zend_function_73;
typedef struct _zend_class_entry_73      zend_class_entry_73;
typedef struct _zend_string_73           zend_string_73;
typedef struct _zend_op_73               zend_op_73;
typedef struct _sapi_globals_struct_73   sapi_globals_struct_73;
typedef struct _sapi_request_info_73     sapi_request_info_73;
typedef struct _php_core_globals_73      php_core_globals_73;
typedef struct _zend_array_73            zend_array_73;
typedef struct _zval_73                  zval_73;
typedef struct _Bucket_73                Bucket_73;

// Assumes 8-byte pointers

                                                // offset   length
                                                //
struct __attribute__((__packed__)) _zend_executor_globals_73 {
    uint8_t pad0[488];                          // 0        +488
    zend_execute_data_73 *current_execute_data; // 488      +8
};

struct __attribute__((__packed__)) _zend_execute_data_73 {
    zend_op_73 *opline;                         // 0        +8
    uint8_t pad0[16];                           // 8        +16
    zend_function_73 *func;                     // 24       +8
    uint8_t pad1[16];                           // 32       +16
    zend_execute_data_73 *prev_execute_data;    // 48       +8
};

union __attribute__((__packed__)) _zend_function_73 {
    uint8_t type;                               // 0        +8
    struct {
        uint8_t pad0[8];                        // 0        +8
        zend_string_73 *function_name;          // 8        +8
        zend_class_entry_73 *scope;             // 16       +8
    } common;
    struct {
        uint8_t pad1[128];                      // 0        +128
        zend_string_73 *filename;               // 128      +8
    } op_array;
};

struct __attribute__((__packed__)) _zend_class_entry_73 {
    uint8_t pad0[8];                            // 0        +8
    zend_function_73 *name;                     // 8        +8
};

struct __attribute__((__packed__)) _zend_string_73 {
    uint8_t pad0[16];                           // 0        +16
    size_t len;                                 // 16       +8
    char val[1];                                // 24       +8
};

struct __attribute__((__packed__)) _zend_op_73 {
    uint8_t pad0[24];                           // 0        +24
    uint32_t lineno;                            // 24       +4
};

struct __attribute__((__packed__)) _sapi_request_info_73 {
    uint8_t pad0[8];                            // 0        +8
    char *query_string;                         // 8        +8
    char *cookie_data;                          // 16       +8
    uint8_t pad1[8];                            // 24       +8
    char *path_translated;                      // 32       +8
    char *request_uri;                          // 40       +8
};

struct __attribute__((__packed__)) _sapi_globals_struct_73 {
    uint8_t pad0[8];                            // 0        +8
    sapi_request_info_73 request_info;          // 8        +48
    uint8_t pad1[384];                          // 56       +384
    double global_request_time;                 // 440      +8
};

struct __attribute__((__packed__)) _zval_73 {
    union {
        int64_t lval;
        double dval;
        zend_string_73 *str;
        zend_array_73 *arr;
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

struct __attribute__((__packed__)) _php_core_globals_73 {
    uint8_t pad0[368];                          // 0        +368
    zval_73 http_globals[6];                    // 368      +48
};

struct __attribute__((__packed__)) _Bucket_73 {
    zval_73 val;                               // 0        +16
    uint64_t h;                                // 16       +8
    zend_string_73 *key;                       // 24       +32
};

struct __attribute__((__packed__)) _zend_array_73 {
    uint8_t pad0[16];                           // 0        +16
    Bucket_73 *arData;                          // 16       +8
    uint32_t nNumUsed;                          // 24       +4
    uint32_t nNumOfElements;                    // 28       +4
    uint32_t nTableSize;                        // 32       +4
};

#endif

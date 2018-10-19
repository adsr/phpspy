#ifndef __php_structs_74_h
#define __php_structs_74_h

#include <stdint.h>

typedef struct _zend_executor_globals_74 zend_executor_globals_74;
typedef struct _zend_execute_data_74     zend_execute_data_74;
typedef struct _zend_op_array_74         zend_op_array_74;
typedef union  _zend_function_74         zend_function_74;
typedef struct _zend_class_entry_74      zend_class_entry_74;
typedef struct _zend_string_74           zend_string_74;
typedef struct _zend_op_74               zend_op_74;
typedef struct _sapi_request_info_74     sapi_request_info_74;
typedef struct _sapi_globals_struct_74   sapi_globals_struct_74;
typedef union  _zend_value_74            zend_value_74;
typedef struct _zval_74                  zval_74;
typedef struct _php_core_globals_74      php_core_globals_74;
typedef struct _Bucket_74                Bucket_74;
typedef struct _zend_array_74            zend_array_74;

/* Assumes 8-byte pointers */
                                                    /* offset   length */
struct __attribute__((__packed__)) _zend_executor_globals_74 {
    uint8_t                 pad0[488];              /* 0        +488 */
    zend_execute_data_74    *current_execute_data;  /* 488      +8 */
};

struct __attribute__((__packed__)) _zend_execute_data_74 {
    zend_op_74              *opline;                /* 0        +8 */
    uint8_t                 pad0[16];               /* 8        +16 */
    zend_function_74        *func;                  /* 24       +8 */
    uint8_t                 pad1[16];               /* 32       +16 */
    zend_execute_data_74    *prev_execute_data;     /* 48       +8 */
    zend_array_74           *symbol_table;          /* 56       +8 */
};

struct __attribute__((__packed__)) _zend_op_array_74 {
    uint8_t                 pad0[52];               /* 0        +52 */
    int                     last_var;               /* 52       +4 */
    uint8_t                 pad1[40];               /* 56       +40 */
    zend_string_74          **vars;                 /* 96       +8 */
    uint8_t                 pad2[32];               /* 104      +32 */
    zend_string_74          *filename;              /* 136      +8 */
    uint32_t                line_start;             /* 144      +4 */
};

union __attribute__((__packed__)) _zend_function_74 {
    uint8_t                 type;                   /* 0        +8 */
    struct {
        uint8_t             pad0[8];                /* 0        +8 */
        zend_string_74      *function_name;         /* 8        +8 */
        zend_class_entry_74 *scope;                 /* 16       +8 */
    } common;
    zend_op_array_74        op_array;               /* 0        +148 */
};

struct __attribute__((__packed__)) _zend_class_entry_74 {
    uint8_t                 pad0[8];                /* 0        +8 */
    zend_string_74          *name;                  /* 8        +8 */
};

struct __attribute__((__packed__)) _zend_string_74 {
    uint8_t                 pad0[16];               /* 0        +16 */
    size_t                  len;                    /* 16       +8 */
    char                    val[1];                 /* 24       +8 */
};

struct __attribute__((__packed__)) _zend_op_74 {
    uint8_t                 pad0[24];               /* 0        +24 */
    uint32_t                lineno;                 /* 24       +4 */
};

struct __attribute__((__packed__)) _sapi_request_info_74 {
    uint8_t                 pad0[8];                /* 0        +8 */
    char                    *query_string;          /* 8        +8 */
    char                    *cookie_data;           /* 16       +8 */
    uint8_t                 pad1[8];                /* 24       +8 */
    char                    *path_translated;       /* 32       +8 */
    char                    *request_uri;           /* 40       +8 */
};

struct __attribute__((__packed__)) _sapi_globals_struct_74 {
    uint8_t                 pad0[8];                /* 0        +8 */
    sapi_request_info_74    request_info;           /* 8        +48 */
    uint8_t                 pad1[384];              /* 56       +384 */
    double                  global_request_time;    /* 440      +8 */
};

union __attribute__((__packed__)) _zend_value_74 {
    long                    lval;                   /* 0        +8 */
    double                  dval;                   /* 0        +8 */
    zend_string_74          *str;                   /* 0        +8 */
    zend_array_74           *arr;                   /* 0        +8 */
};

struct __attribute__((__packed__)) _zval_74 {
    zend_value_74           value;                  /* 0        +8 */
    union {
        struct {
            uint8_t         type;                   /* 8        +1 */
            uint8_t         pad0[3];                /* 9        +3 */
        } v;
    } u1;
    uint8_t                 pad1[4];                /* 12       +4 */
};

struct __attribute__((__packed__)) _php_core_globals_74 {
    uint8_t                 pad0[368];              /* 0        +368 */
    zval_74                 http_globals[6];        /* 368      +48 */
};

struct __attribute__((__packed__)) _zend_array_74 {
    uint8_t                 pad0[16];               /* 0        +16 */
    Bucket_74               *arData;                /* 16       +8 */
    uint32_t                nNumUsed;               /* 24       +4 */
    uint32_t                nNumOfElements;         /* 28       +4 */
    uint32_t                nTableSize;             /* 32       +4 */
};

struct __attribute__((__packed__)) _Bucket_74 {
    zval_74                 val;                    /* 0        +16 */
    uint64_t                h;                      /* 16       +8 */
    zend_string_74          *key;                   /* 24       +32 */
};

#endif

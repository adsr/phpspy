#ifndef __php_structs_73_h
#define __php_structs_73_h

#include <stdint.h>

typedef struct _zend_executor_globals_73 zend_executor_globals_73;
typedef struct _zend_execute_data_73     zend_execute_data_73;
typedef struct _zend_op_array_73         zend_op_array_73;
typedef union  _zend_function_73         zend_function_73;
typedef struct _zend_class_entry_73      zend_class_entry_73;
typedef struct _zend_string_73           zend_string_73;
typedef struct _zend_op_73               zend_op_73;
typedef struct _sapi_request_info_73     sapi_request_info_73;
typedef struct _sapi_globals_struct_73   sapi_globals_struct_73;
typedef union  _zend_value_73            zend_value_73;
typedef struct _zval_73                  zval_73;
typedef struct _Bucket_73                Bucket_73;
typedef struct _zend_array_73            zend_array_73;
typedef struct _zend_alloc_globals_73    zend_alloc_globals_73;
typedef struct _zend_mm_heap_73          zend_mm_heap_73;

/* Assumes 8-byte pointers */
                                                    /* offset   length */
struct __attribute__((__packed__)) _zend_array_73 {
    uint8_t                 pad0[12];               /* 0        +12 */
    uint32_t                nTableMask;             /* 12       +4 */
    Bucket_73               *arData;                /* 16       +8 */
    uint32_t                nNumUsed;               /* 24       +4 */
    uint32_t                nNumOfElements;         /* 28       +4 */
    uint32_t                nTableSize;             /* 32       +4 */
};

struct __attribute__((__packed__)) _zend_executor_globals_73 {
    uint8_t                 pad0[304];              /* 0        +304 */
    zend_array_73           symbol_table;           /* 304      +36 */
    uint8_t                 pad1[148];              /* 340      +148 */
    zend_execute_data_73    *current_execute_data;  /* 488      +8 */
};

struct __attribute__((__packed__)) _zend_execute_data_73 {
    zend_op_73              *opline;                /* 0        +8 */
    uint8_t                 pad0[16];               /* 8        +16 */
    zend_function_73        *func;                  /* 24       +8 */
    uint8_t                 pad1[16];               /* 32       +16 */
    zend_execute_data_73    *prev_execute_data;     /* 48       +8 */
    zend_array_73           *symbol_table;          /* 56       +8 */
};

struct __attribute__((__packed__)) _zend_op_array_73 {
    uint8_t                 pad0[52];               /* 0        +52 */
    int                     last_var;               /* 52       +4 */
    uint8_t                 pad1[32];               /* 56       +32 */
    zend_string_73          **vars;                 /* 88       +8 */
    uint8_t                 pad2[32];               /* 96       +32 */
    zend_string_73          *filename;              /* 128      +8 */
    uint32_t                line_start;             /* 136      +4 */
};

union __attribute__((__packed__)) _zend_function_73 {
    uint8_t                 type;                   /* 0        +1 */
    struct {
        uint8_t             pad0[8];                /* 0        +8 */
        zend_string_73      *function_name;         /* 8        +8 */
        zend_class_entry_73 *scope;                 /* 16       +8 */
    } common;
    zend_op_array_73        op_array;               /* 0        +216 */
};

struct __attribute__((__packed__)) _zend_class_entry_73 {
    uint8_t                 pad0[8];                /* 0        +8 */
    zend_string_73          *name;                  /* 8        +8 */
};

struct __attribute__((__packed__)) _zend_string_73 {
    uint8_t                 pad0[16];               /* 0        +16 */
    size_t                  len;                    /* 16       +8 */
    char                    val[1];                 /* 24       +1 */
};

struct __attribute__((__packed__)) _zend_op_73 {
    uint8_t                 pad0[24];               /* 0        +24 */
    uint32_t                lineno;                 /* 24       +4 */
};

struct __attribute__((__packed__)) _sapi_request_info_73 {
    uint8_t                 pad0[8];                /* 0        +8 */
    char                    *query_string;          /* 8        +8 */
    char                    *cookie_data;           /* 16       +8 */
    uint8_t                 pad1[8];                /* 24       +8 */
    char                    *path_translated;       /* 32       +8 */
    char                    *request_uri;           /* 40       +8 */
};

struct __attribute__((__packed__)) _sapi_globals_struct_73 {
    uint8_t                 pad0[8];                /* 0        +8 */
    sapi_request_info_73    request_info;           /* 8        +48 */
    uint8_t                 pad1[384];              /* 56       +384 */
    double                  global_request_time;    /* 440      +8 */
};

union __attribute__((__packed__)) _zend_value_73 {
    long                    lval;                   /* 0        +8 */
    double                  dval;                   /* 0        +8 */
    zend_string_73          *str;                   /* 0        +8 */
    zend_array_73           *arr;                   /* 0        +8 */
};

struct __attribute__((__packed__)) _zval_73 {
    zend_value_73           value;                  /* 0        +8 */
    union {
        struct {
            uint8_t         type;                   /* 8        +1 */
            uint8_t         pad0[3];                /* 9        +3 */
        } v;
    } u1;
    union {
        uint32_t next;                              /* 12       +4 */
    } u2;
};

struct __attribute__((__packed__)) _Bucket_73 {
    zval_73                 val;                    /* 0        +16 */
    uint64_t                h;                      /* 16       +8 */
    zend_string_73          *key;                   /* 24       +8 */
};

struct __attribute__((__packed__)) _zend_alloc_globals_73 {
    zend_mm_heap_73         *mm_heap;               /* 0        +8 */
};

struct __attribute__((__packed__)) _zend_mm_heap_73 {
    uint8_t                 pad0[16];               /* 0        +16 */
    size_t                  size;                   /* 16       +8 */
    size_t                  peak;                   /* 24       +8 */
};

#endif

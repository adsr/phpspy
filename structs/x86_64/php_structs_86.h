#ifndef __php_structs_86_h
#define __php_structs_86_h

#include <stdint.h>

typedef struct _zend_executor_globals_86 zend_executor_globals_86;
typedef struct _zend_execute_data_86     zend_execute_data_86;
typedef struct _zend_op_array_86         zend_op_array_86;
typedef union  _zend_function_86         zend_function_86;
typedef struct _zend_class_entry_86      zend_class_entry_86;
typedef struct _zend_string_86           zend_string_86;
typedef struct _zend_op_86               zend_op_86;
typedef struct _sapi_request_info_86     sapi_request_info_86;
typedef struct _sapi_globals_struct_86   sapi_globals_struct_86;
typedef union  _zend_value_86            zend_value_86;
typedef struct _zval_86                  zval_86;
typedef struct _Bucket_86                Bucket_86;
typedef struct _zend_array_86            zend_array_86;
typedef struct _zend_alloc_globals_86    zend_alloc_globals_86;
typedef struct _zend_mm_heap_86          zend_mm_heap_86;

/* Assumes 8-byte pointers */
                                                    /* offset   length */
struct __attribute__((__packed__)) _zend_array_86 {
    uint8_t                 pad0[12];               /* 0        +12 */
    uint32_t                nTableMask;             /* 12       +4 */
    Bucket_86               *arData;                /* 16       +8 */
    uint32_t                nNumUsed;               /* 24       +4 */
    uint32_t                nNumOfElements;         /* 28       +4 */
    uint32_t                nTableSize;             /* 32       +4 */
};

struct __attribute__((__packed__)) _zend_executor_globals_86 {
    uint8_t                 pad0[304];              /* 0        +304 */
    zend_array_86           symbol_table;           /* 304      +36 */
    uint8_t                 pad1[172];              /* 340      +172 */
    zend_execute_data_86    *current_execute_data;  /* 512      +8 */
};

struct __attribute__((__packed__)) _zend_execute_data_86 {
    zend_op_86              *opline;                /* 0        +8 */
    uint8_t                 pad0[16];               /* 8        +16 */
    zend_function_86        *func;                  /* 24       +8 */
    uint8_t                 pad1[16];               /* 32       +16 */
    zend_execute_data_86    *prev_execute_data;     /* 48       +8 */
    zend_array_86           *symbol_table;          /* 56       +8 */
};

struct __attribute__((__packed__)) _zend_op_array_86 {
    uint8_t                 pad0[92];               /* 0        +92 */
    int                     last_var;               /* 92       +4 */
    uint8_t                 pad1[32];               /* 96       +32 */
    zend_string_86          **vars;                 /* 128      +8 */
    uint8_t                 pad2[32];               /* 136      +32 */
    zend_string_86          *filename;              /* 168      +8 */
    uint32_t                line_start;             /* 176      +4 */
};

union __attribute__((__packed__)) _zend_function_86 {
    uint8_t                 type;                   /* 0        +1 */
    struct {
        uint8_t             pad0[8];                /* 0        +8 */
        zend_string_86      *function_name;         /* 8        +8 */
        zend_class_entry_86 *scope;                 /* 16       +8 */
    } common;
    zend_op_array_86        op_array;               /* 0        +240 */
};

struct __attribute__((__packed__)) _zend_class_entry_86 {
    uint8_t                 pad0[8];                /* 0        +8 */
    zend_string_86          *name;                  /* 8        +8 */
};

struct __attribute__((__packed__)) _zend_string_86 {
    uint8_t                 pad0[16];               /* 0        +16 */
    size_t                  len;                    /* 16       +8 */
    char                    val[1];                 /* 24       +1 */
};

struct __attribute__((__packed__)) _zend_op_86 {
    uint8_t                 pad0[24];               /* 0        +24 */
    uint32_t                lineno;                 /* 24       +4 */
};

struct __attribute__((__packed__)) _sapi_request_info_86 {
    uint8_t                 pad0[8];                /* 0        +8 */
    char                    *query_string;          /* 8        +8 */
    char                    *cookie_data;           /* 16       +8 */
    uint8_t                 pad1[8];                /* 24       +8 */
    char                    *path_translated;       /* 32       +8 */
    char                    *request_uri;           /* 40       +8 */
};

struct __attribute__((__packed__)) _sapi_globals_struct_86 {
    uint8_t                 pad0[8];                /* 0        +8 */
    sapi_request_info_86    request_info;           /* 8        +48 */
    uint8_t                 pad1[384];              /* 56       +384 */
    double                  global_request_time;    /* 440      +8 */
};

union __attribute__((__packed__)) _zend_value_86 {
    long                    lval;                   /* 0        +8 */
    double                  dval;                   /* 0        +8 */
    zend_string_86          *str;                   /* 0        +8 */
    zend_array_86           *arr;                   /* 0        +8 */
};

struct __attribute__((__packed__)) _zval_86 {
    zend_value_86           value;                  /* 0        +8 */
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

struct __attribute__((__packed__)) _Bucket_86 {
    zval_86                 val;                    /* 0        +16 */
    uint64_t                h;                      /* 16       +8 */
    zend_string_86          *key;                   /* 24       +8 */
};

struct __attribute__((__packed__)) _zend_alloc_globals_86 {
    zend_mm_heap_86         *mm_heap;               /* 0        +8 */
};

struct __attribute__((__packed__)) _zend_mm_heap_86 {
    uint8_t                 pad0[16];               /* 0        +16 */
    size_t                  size;                   /* 16       +8 */
    size_t                  peak;                   /* 24       +8 */
};

#endif

#ifndef __php_structs_85_h
#define __php_structs_85_h

#include <stdint.h>

typedef struct _zend_executor_globals_85 zend_executor_globals_85;
typedef struct _zend_execute_data_85     zend_execute_data_85;
typedef struct _zend_op_array_85         zend_op_array_85;
typedef union  _zend_function_85         zend_function_85;
typedef struct _zend_class_entry_85      zend_class_entry_85;
typedef struct _zend_string_85           zend_string_85;
typedef struct _zend_op_85               zend_op_85;
typedef struct _sapi_request_info_85     sapi_request_info_85;
typedef struct _sapi_globals_struct_85   sapi_globals_struct_85;
typedef union  _zend_value_85            zend_value_85;
typedef struct _zval_85                  zval_85;
typedef struct _Bucket_85                Bucket_85;
typedef struct _zend_array_85            zend_array_85;
typedef struct _zend_alloc_globals_85    zend_alloc_globals_85;
typedef struct _zend_mm_heap_85          zend_mm_heap_85;

/* Assumes 8-byte pointers */
                                                    /* offset   length */
struct __attribute__((__packed__)) _zend_array_85 {
    uint8_t                 pad0[12];               /* 0        +12 */
    uint32_t                nTableMask;             /* 12       +4 */
    Bucket_85               *arData;                /* 16       +8 */
    uint32_t                nNumUsed;               /* 24       +4 */
    uint32_t                nNumOfElements;         /* 28       +4 */
    uint32_t                nTableSize;             /* 32       +4 */
};

struct __attribute__((__packed__)) _zend_executor_globals_85 {
    uint8_t                 pad0[304];              /* 0        +304 */
    zend_array_85           symbol_table;           /* 304      +36 */
    uint8_t                 pad1[172];              /* 340      +172 */
    zend_execute_data_85    *current_execute_data;  /* 512      +8 */
};

struct __attribute__((__packed__)) _zend_execute_data_85 {
    zend_op_85              *opline;                /* 0        +8 */
    uint8_t                 pad0[16];               /* 8        +16 */
    zend_function_85        *func;                  /* 24       +8 */
    uint8_t                 pad1[16];               /* 32       +16 */
    zend_execute_data_85    *prev_execute_data;     /* 48       +8 */
    zend_array_85           *symbol_table;          /* 56       +8 */
};

struct __attribute__((__packed__)) _zend_op_array_85 {
    uint8_t                 pad0[92];               /* 0        +92 */
    int                     last_var;               /* 92       +4 */
    uint8_t                 pad1[32];               /* 96       +32 */
    zend_string_85          **vars;                 /* 128      +8 */
    uint8_t                 pad2[32];               /* 136      +32 */
    zend_string_85          *filename;              /* 168      +8 */
    uint32_t                line_start;             /* 176      +4 */
};

union __attribute__((__packed__)) _zend_function_85 {
    uint8_t                 type;                   /* 0        +1 */
    struct {
        uint8_t             pad0[8];                /* 0        +8 */
        zend_string_85      *function_name;         /* 8        +8 */
        zend_class_entry_85 *scope;                 /* 16       +8 */
    } common;
    zend_op_array_85        op_array;               /* 0        +240 */
};

struct __attribute__((__packed__)) _zend_class_entry_85 {
    uint8_t                 pad0[8];                /* 0        +8 */
    zend_string_85          *name;                  /* 8        +8 */
};

struct __attribute__((__packed__)) _zend_string_85 {
    uint8_t                 pad0[16];               /* 0        +16 */
    size_t                  len;                    /* 16       +8 */
    char                    val[1];                 /* 24       +1 */
};

struct __attribute__((__packed__)) _zend_op_85 {
    uint8_t                 pad0[24];               /* 0        +24 */
    uint32_t                lineno;                 /* 24       +4 */
};

struct __attribute__((__packed__)) _sapi_request_info_85 {
    uint8_t                 pad0[8];                /* 0        +8 */
    char                    *query_string;          /* 8        +8 */
    char                    *cookie_data;           /* 16       +8 */
    uint8_t                 pad1[8];                /* 24       +8 */
    char                    *path_translated;       /* 32       +8 */
    char                    *request_uri;           /* 40       +8 */
};

struct __attribute__((__packed__)) _sapi_globals_struct_85 {
    uint8_t                 pad0[8];                /* 0        +8 */
    sapi_request_info_85    request_info;           /* 8        +48 */
    uint8_t                 pad1[368];              /* 56       +368 */
    double                  global_request_time;    /* 424      +8 */
};

union __attribute__((__packed__)) _zend_value_85 {
    long                    lval;                   /* 0        +8 */
    double                  dval;                   /* 0        +8 */
    zend_string_85          *str;                   /* 0        +8 */
    zend_array_85           *arr;                   /* 0        +8 */
};

struct __attribute__((__packed__)) _zval_85 {
    zend_value_85           value;                  /* 0        +8 */
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

struct __attribute__((__packed__)) _Bucket_85 {
    zval_85                 val;                    /* 0        +16 */
    uint64_t                h;                      /* 16       +8 */
    zend_string_85          *key;                   /* 24       +8 */
};

struct __attribute__((__packed__)) _zend_alloc_globals_85 {
    zend_mm_heap_85         *mm_heap;               /* 0        +8 */
};

struct __attribute__((__packed__)) _zend_mm_heap_85 {
    uint8_t                 pad0[16];               /* 0        +16 */
    size_t                  size;                   /* 16       +8 */
    size_t                  peak;                   /* 24       +8 */
};

#endif

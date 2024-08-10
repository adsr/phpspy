#ifndef __php_structs_82_h
#define __php_structs_82_h

#include <stdint.h>

typedef struct _zend_executor_globals_82 zend_executor_globals_82;
typedef struct _zend_execute_data_82     zend_execute_data_82;
typedef struct _zend_op_array_82         zend_op_array_82;
typedef union  _zend_function_82         zend_function_82;
typedef struct _zend_class_entry_82      zend_class_entry_82;
typedef struct _zend_string_82           zend_string_82;
typedef struct _zend_op_82               zend_op_82;
typedef struct _sapi_request_info_82     sapi_request_info_82;
typedef struct _sapi_globals_struct_82   sapi_globals_struct_82;
typedef union  _zend_value_82            zend_value_82;
typedef struct _zval_82                  zval_82;
typedef struct _Bucket_82                Bucket_82;
typedef struct _zend_array_82            zend_array_82;
typedef struct _zend_alloc_globals_82    zend_alloc_globals_82;
typedef struct _zend_mm_heap_82          zend_mm_heap_82;

#ifdef WINDOWS
#pragma pack(push, 1)
#endif

/* Assumes 8-byte pointers */
                                                    /* offset   length */
struct PHPSPY_PACK _zend_array_82 {
    uint8_t                 pad0[12];               /* 0        +12 */
    uint32_t                nTableMask;             /* 12       +4 */
    Bucket_82               *arData;                /* 16       +8 */
    uint32_t                nNumUsed;               /* 24       +4 */
    uint32_t                nNumOfElements;         /* 28       +4 */
    uint32_t                nTableSize;             /* 32       +4 */
};

struct PHPSPY_PACK _zend_executor_globals_82 {
    uint8_t                 pad0[304];              /* 0        +304 */
    zend_array_82           symbol_table;           /* 304      +36 */
    uint8_t                 pad1[148];              /* 340      +148 */
    zend_execute_data_82    *current_execute_data;  /* 488      +8 */
};

struct PHPSPY_PACK _zend_execute_data_82 {
    zend_op_82              *opline;                /* 0        +8 */
    uint8_t                 pad0[16];               /* 8        +16 */
    zend_function_82        *func;                  /* 24       +8 */
    uint8_t                 pad1[16];               /* 32       +16 */
    zend_execute_data_82    *prev_execute_data;     /* 48       +8 */
    zend_array_82           *symbol_table;          /* 56       +8 */
};

struct PHPSPY_PACK _zend_op_array_82 {
    uint8_t                 pad0[76];               /* 0        +76 */
    int                     last_var;               /* 76       +4 */
    uint8_t                 pad1[32];               /* 80       +32 */
    zend_string_82          **vars;                 /* 112      +8 */
    uint8_t                 pad2[32];               /* 120      +32 */
    zend_string_82          *filename;              /* 152      +8 */
    uint32_t                line_start;             /* 160      +4 */
};

union PHPSPY_PACK _zend_function_82 {
    uint8_t                 type;                   /* 0        +1 */
    struct {
        uint8_t             pad0[8];                /* 0        +8 */
        zend_string_82      *function_name;         /* 8        +8 */
        zend_class_entry_82 *scope;                 /* 16       +8 */
    } common;
    zend_op_array_82        op_array;               /* 0        +248 */
};

struct PHPSPY_PACK _zend_class_entry_82 {
    uint8_t                 pad0[8];                /* 0        +8 */
    zend_string_82          *name;                  /* 8        +8 */
};

struct PHPSPY_PACK _zend_string_82 {
    uint8_t                 pad0[16];               /* 0        +16 */
    size_t                  len;                    /* 16       +8 */
    char                    val[1];                 /* 24       +1 */
};

struct PHPSPY_PACK _zend_op_82 {
    uint8_t                 pad0[24];               /* 0        +24 */
    uint32_t                lineno;                 /* 24       +4 */
};

struct PHPSPY_PACK _sapi_request_info_82 {
    uint8_t                 pad0[8];                /* 0        +8 */
    char                    *query_string;          /* 8        +8 */
    char                    *cookie_data;           /* 16       +8 */
    uint8_t                 pad1[8];                /* 24       +8 */
    char                    *path_translated;       /* 32       +8 */
    char                    *request_uri;           /* 40       +8 */
};

struct PHPSPY_PACK _sapi_globals_struct_82 {
    uint8_t                 pad0[8];                /* 0        +8 */
    sapi_request_info_82    request_info;           /* 8        +48 */
    uint8_t                 pad1[384];              /* 56       +384 */
    double                  global_request_time;    /* 440      +8 */
};

union PHPSPY_PACK _zend_value_82 {
    long                    lval;                   /* 0        +8 */
    double                  dval;                   /* 0        +8 */
    zend_string_82          *str;                   /* 0        +8 */
    zend_array_82           *arr;                   /* 0        +8 */
};

struct PHPSPY_PACK _zval_82 {
    zend_value_82           value;                  /* 0        +8 */
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

struct PHPSPY_PACK _Bucket_82 {
    zval_82                 val;                    /* 0        +16 */
    uint64_t                h;                      /* 16       +8 */
    zend_string_82          *key;                   /* 24       +8 */
};

struct PHPSPY_PACK _zend_alloc_globals_82 {
    zend_mm_heap_82         *mm_heap;               /* 0        +8 */
};

struct PHPSPY_PACK _zend_mm_heap_82 {
    uint8_t                 pad0[16];               /* 0        +16 */
    size_t                  size;                   /* 16       +8 */
    size_t                  peak;                   /* 24       +8 */
};

#ifdef WINDOWS
#pragma pack(pop)
#endif

#endif

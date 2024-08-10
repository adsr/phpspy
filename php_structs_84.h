#ifndef __php_structs_84_h
#define __php_structs_84_h

#include <stdint.h>

typedef struct _zend_executor_globals_84 zend_executor_globals_84;
typedef struct _zend_execute_data_84     zend_execute_data_84;
typedef struct _zend_op_array_84         zend_op_array_84;
typedef union  _zend_function_84         zend_function_84;
typedef struct _zend_class_entry_84      zend_class_entry_84;
typedef struct _zend_string_84           zend_string_84;
typedef struct _zend_op_84               zend_op_84;
typedef struct _sapi_request_info_84     sapi_request_info_84;
typedef struct _sapi_globals_struct_84   sapi_globals_struct_84;
typedef union  _zend_value_84            zend_value_84;
typedef struct _zval_84                  zval_84;
typedef struct _Bucket_84                Bucket_84;
typedef struct _zend_array_84            zend_array_84;
typedef struct _zend_alloc_globals_84    zend_alloc_globals_84;
typedef struct _zend_mm_heap_84          zend_mm_heap_84;


#ifdef WINDOWS
#pragma pack(push, 1)
#endif

/* Assumes 8-byte pointers */
                                                    /* offset   length */
struct PHPSPY_PACK _zend_array_84 {
    uint8_t                 pad0[12];               /* 0        +12 */
    uint32_t                nTableMask;             /* 12       +4 */
    Bucket_84               *arData;                /* 16       +8 */
    uint32_t                nNumUsed;               /* 24       +4 */
    uint32_t                nNumOfElements;         /* 28       +4 */
    uint32_t                nTableSize;             /* 32       +4 */
};

struct PHPSPY_PACK _zend_executor_globals_84 {
    uint8_t                 pad0[304];              /* 0        +304 */
    zend_array_84           symbol_table;           /* 304      +36 */
    uint8_t                 pad1[148];              /* 340      +148 */
    zend_execute_data_84    *current_execute_data;  /* 488      +8 */
};

struct PHPSPY_PACK _zend_execute_data_84 {
    zend_op_84              *opline;                /* 0        +8 */
    uint8_t                 pad0[16];               /* 8        +16 */
    zend_function_84        *func;                  /* 24       +8 */
    uint8_t                 pad1[16];               /* 32       +16 */
    zend_execute_data_84    *prev_execute_data;     /* 48       +8 */
    zend_array_84           *symbol_table;          /* 56       +8 */
};

struct PHPSPY_PACK _zend_op_array_84 {
    uint8_t                 pad0[80];               /* 0        +80 */
    int                     last_var;               /* 80       +4 */
    uint8_t                 pad1[28];               /* 84       +28 */
    zend_string_84          **vars;                 /* 112      +8 */
    uint8_t                 pad2[32];               /* 120      +32 */
    zend_string_84          *filename;              /* 152      +8 */
    uint32_t                line_start;             /* 160      +4 */
};

union PHPSPY_PACK _zend_function_84 {
    uint8_t                 type;                   /* 0        +1 */
    struct {
        uint8_t             pad0[8];                /* 0        +8 */
        zend_string_84      *function_name;         /* 8        +8 */
        zend_class_entry_84 *scope;                 /* 16       +8 */
    } common;
    zend_op_array_84        op_array;               /* 0        +240 */
};

struct PHPSPY_PACK _zend_class_entry_84 {
    uint8_t                 pad0[8];                /* 0        +8 */
    zend_string_84          *name;                  /* 8        +8 */
};

struct PHPSPY_PACK _zend_string_84 {
    uint8_t                 pad0[16];               /* 0        +16 */
    size_t                  len;                    /* 16       +8 */
    char                    val[1];                 /* 24       +1 */
};

struct PHPSPY_PACK _zend_op_84 {
    uint8_t                 pad0[24];               /* 0        +24 */
    uint32_t                lineno;                 /* 24       +4 */
};

struct PHPSPY_PACK _sapi_request_info_84 {
    uint8_t                 pad0[8];                /* 0        +8 */
    char                    *query_string;          /* 8        +8 */
    char                    *cookie_data;           /* 16       +8 */
    uint8_t                 pad1[8];                /* 24       +8 */
    char                    *path_translated;       /* 32       +8 */
    char                    *request_uri;           /* 40       +8 */
};

struct PHPSPY_PACK _sapi_globals_struct_84 {
    uint8_t                 pad0[8];                /* 0        +8 */
    sapi_request_info_84    request_info;           /* 8        +48 */
    uint8_t                 pad1[384];              /* 56       +384 */
    double                  global_request_time;    /* 440      +8 */
};

union PHPSPY_PACK _zend_value_84 {
    long                    lval;                   /* 0        +8 */
    double                  dval;                   /* 0        +8 */
    zend_string_84          *str;                   /* 0        +8 */
    zend_array_84           *arr;                   /* 0        +8 */
};

struct PHPSPY_PACK _zval_84 {
    zend_value_84           value;                  /* 0        +8 */
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

struct PHPSPY_PACK _Bucket_84 {
    zval_84                 val;                    /* 0        +16 */
    uint64_t                h;                      /* 16       +8 */
    zend_string_84          *key;                   /* 24       +8 */
};

struct PHPSPY_PACK _zend_alloc_globals_84 {
    zend_mm_heap_84         *mm_heap;               /* 0        +8 */
};

struct PHPSPY_PACK _zend_mm_heap_84 {
    uint8_t                 pad0[16];               /* 0        +16 */
    size_t                  size;                   /* 16       +8 */
    size_t                  peak;                   /* 24       +8 */
};

#ifdef WINDOWS
#pragma pack(pop)
#endif

#endif

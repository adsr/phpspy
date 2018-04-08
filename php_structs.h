#ifndef __struct_h
#define __struct_h

#include <stdint.h>

typedef struct _zend_executor_globals zend_executor_globals;
typedef struct _zend_execute_data zend_execute_data;
typedef union  _zend_function zend_function;
typedef struct _zend_string zend_string;
typedef struct _zend_op zend_op;

// Assumes 8-byte pointers

                                            // offset   length
                                            //
struct __attribute__((__packed__)) _zend_executor_globals {
    uint8_t pad0[480];                      // 0        480
    zend_execute_data *current_execute_data;// 480      8
};

struct __attribute__((__packed__)) _zend_execute_data {
    zend_op *opline;                        // 0        8
    uint8_t pad0[16];                       // 8        16
    zend_function *func;                    // 24       8
    uint8_t pad1[16];                       // 32       16
    zend_execute_data *prev_execute_data;   // 48       8
};

union __attribute__((__packed__)) _zend_function {
    uint8_t type;                           // 0        8
    struct {
        uint8_t pad0[8];                    // 0        8
        zend_string *function_name;         // 8        8
    } common;
    struct {
        uint8_t pad1[120];                  // 0        120
        zend_string *filename;              // 120      8
    } op_array;
};

struct __attribute__((__packed__)) _zend_op {
    uint8_t pad0[24];                       // 0        24
    uint32_t lineno;                        // 24       4
};

struct __attribute__((__packed__)) _zend_string {
    uint8_t pad0[16];                       // 0        16
    size_t len;                             // 16       8
    char *val;                              // 24       8
};

#endif

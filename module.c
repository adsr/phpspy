/*
    Requires CMake build of phpspy (for headers)
    
    compile: gcc -shared -fPIC -Wl,-init,phpspy_module_init ../module.c -I/opt/include/phpspy -o module.so
    run    : sudo LD_PRELOAD=./module.so /opt/bin/phpspy -p PID
    
    TODO:
        make cmake build for module
*/

#include <phpspy.h>

int phpspy_module_handler(trace_context_t *context, int type) {
    fprintf(stderr, "event: %d\n", type);
    return 0;
}

void phpspy_module_init() {
    phpspy_event_handler = phpspy_module_handler;
}

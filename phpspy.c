#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/uio.h>
#include <time.h>

#ifdef USE_ZEND
#include <zend_API.h>
#else
#include <php_structs.h>
#endif

pid_t opt_pid = -1;
int opt_sleep_us = 10000;
unsigned long long opt_executor_globals_addr = 0;
int opt_max_stack_depth = -1;
char *opt_frame_delim = "\n";
char *opt_trace_delim = "";

size_t zend_string_val_offset;

static void dump_trace(pid_t pid, unsigned long long executor_globals_addr);
static int copy_proc_mem(pid_t pid, void *raddr, void *laddr, size_t size);
static void parse_opts(int argc, char **argv);
static int get_php_bin_path(pid_t pid, char *path);
static int get_php_base_addr(pid_t pid, char *path, unsigned long long *raddr);
static int get_executor_globals_offset(char *path, unsigned long long *raddr);
static int get_executor_globals_addr(unsigned long long *raddr);
static int popen_read_line(char *buf, size_t buf_size, char *cmd_fmt, ...);

static void usage(FILE *fp, int exit_code) {
    fprintf(fp, "Usage: phpspy -h(help) -p<pid> -s<sleep_us> -n<max_stack_depth> -x<executor_globals_addr>\n");
    exit(exit_code);
}

int main(int argc, char **argv) {
    unsigned long long executor_globals_addr;
    struct timespec start_time;
    struct timespec end_time;
    struct timespec sleep_time;
    long start_time_high_resolution;
    long end_time_high_resolution;
    long opt_sleep_nano;

    parse_opts(argc, argv);

    if (get_executor_globals_addr(&executor_globals_addr) != 0) {
        return 1;
    }

    zend_string_val_offset = offsetof(zend_string, val);

    while (1) {
        if (clock_gettime(CLOCK_MONOTONIC, &start_time) == -1) {
          perror("clock_gettime failed:");
        }
        start_time_high_resolution = (start_time.tv_sec * 1000000000ull) + start_time.tv_nsec;

        dump_trace(opt_pid, executor_globals_addr);

        if (clock_gettime(CLOCK_MONOTONIC, &end_time) == -1) {
          perror("clock_gettime failed:");
        }
        end_time_high_resolution = (end_time.tv_sec * 1000000000ull) + end_time.tv_nsec;

        opt_sleep_nano = opt_sleep_us * 1000;
        sleep_time.tv_nsec = opt_sleep_nano - (end_time_high_resolution - start_time_high_resolution);

        nanosleep(&sleep_time, NULL);
    }

    return 0;
}

static void dump_trace(pid_t pid, unsigned long long executor_globals_addr) {
    char func[1024];
    char file[1024];
    char class[1024];
    int file_len;
    int func_len;
    int class_len;
    int lineno;
    int depth;
    int wrote_trace;
    zend_execute_data* remote_execute_data;
    zend_executor_globals executor_globals;
    zend_execute_data execute_data;
    zend_op zop;
    zend_class_entry zce;
    zend_function zfunc;
    zend_string zstring;

    depth = 0;
    wrote_trace = 0;

    #define try_copy_proc_mem(__what, __raddr, __laddr, __size) do {        \
        if (copy_proc_mem(pid, (__raddr), (__laddr), (__size)) != 0) {      \
            fprintf(stderr, "dump_trace: Failed to copy %s\n", (__what));   \
            if (wrote_trace) printf("%s", opt_trace_delim);                 \
            return;                                                         \
        }                                                                   \
    } while(0)

    executor_globals.current_execute_data = NULL;
    try_copy_proc_mem("executor_globals", (void*)executor_globals_addr, &executor_globals, sizeof(executor_globals));
    remote_execute_data = executor_globals.current_execute_data;

    while (remote_execute_data && depth != opt_max_stack_depth) {
        memset(&execute_data, 0, sizeof(execute_data));
        memset(&zfunc, 0, sizeof(zfunc));
        memset(&zstring, 0, sizeof(zstring));
        memset(&zce, 0, sizeof(zce));
        memset(&zop, 0, sizeof(zop));

        try_copy_proc_mem("execute_data", remote_execute_data, &execute_data, sizeof(execute_data));
        try_copy_proc_mem("zfunc", execute_data.func, &zfunc, sizeof(zfunc));
        if (zfunc.common.function_name) {
            try_copy_proc_mem("function_name", zfunc.common.function_name, &zstring, sizeof(zstring));
            func_len = zstring.len;
            try_copy_proc_mem("function_name.val", ((char*)zfunc.common.function_name) + zend_string_val_offset, func, func_len);
        } else {
            func_len = snprintf(func, sizeof(func), "<main>");
        }
        if (zfunc.common.scope) {
            try_copy_proc_mem("zce", zfunc.common.scope, &zce, sizeof(zce));
            try_copy_proc_mem("class_name", zce.name, &zstring, sizeof(zstring));
            class_len = zstring.len;
            try_copy_proc_mem("class_name.val", ((char*)zce.name) + zend_string_val_offset, class, class_len);
            if (class_len+2 <= 1023) {
                class[class_len+0] = ':';
                class[class_len+1] = ':';
                class[class_len+2] = '\0';
                class_len += 2;
            }
        } else {
            class[0] = '\0';
            class_len = 0;
        }
        if (zfunc.type == 2) {
            try_copy_proc_mem("zop", (void*)execute_data.opline, &zop, sizeof(zop));
            try_copy_proc_mem("filename", zfunc.op_array.filename, &zstring, sizeof(zstring));
            file_len = zstring.len;
            try_copy_proc_mem("filename.val", ((char*)zfunc.op_array.filename) + zend_string_val_offset, file, file_len);
            lineno = zop.lineno;
        } else {
            file_len = snprintf(file, sizeof(file), "<internal>");
            lineno = -1;
        }
        printf("%d %.*s%.*s %.*s:%d%s", depth, class_len, class, func_len, func, file_len, file, lineno, opt_frame_delim);
        if (!wrote_trace) wrote_trace = 1;
        remote_execute_data = execute_data.prev_execute_data;
        depth += 1;
    }
    if (wrote_trace) printf("%s", opt_trace_delim);
}

static int copy_proc_mem(pid_t pid, void *raddr, void *laddr, size_t size) {
    struct iovec local[1];
    struct iovec remote[1];
    local[0].iov_base = laddr;
    local[0].iov_len = size;
    remote[0].iov_base = raddr;
    remote[0].iov_len = size;
    if (process_vm_readv(pid, local, 1, remote, 1, 0) == -1) {
        fprintf(stderr, "copy_proc_mem: %s; raddr=%p size=%lu\n", strerror(errno), raddr, size);
        return 1;
    }
    return 0;
}

static void parse_opts(int argc, char **argv) {
    int c;
    while ((c = getopt(argc, argv, "hp:s:n:x:")) != -1) {
        switch (c) {
            case 'h': usage(stdout, 0); break;
            case 'p': opt_pid = atoi(optarg); break;
            case 's': opt_sleep_us = atoi(optarg); break;
            case 'n': opt_max_stack_depth = atoi(optarg); break;
            case 'x': opt_executor_globals_addr = strtoull(optarg, NULL, 16); break;
        }
    }
    if (opt_pid == -1) {
        if (optind < argc) {
            opt_pid = atoi(argv[optind]);
        } else {
            fprintf(stderr, "parse_opts: Expected pid\n\n");
            usage(stderr, 1);
        }
    }
}

static int get_php_bin_path(pid_t pid, char *path) {
    char buf[128];
    char *cmd_fmt = "awk 'NR==1{path=$NF} /libphp7/{path=$NF} END{print path}' /proc/%d/maps";
    if (popen_read_line(buf, sizeof(buf), cmd_fmt, (int)pid) != 0) {
        fprintf(stderr, "get_php_bin_path: Failed\n");
        return 1;
    }
    strcpy(path, buf);
    return 0;
}

static int get_php_base_addr(pid_t pid, char *path, unsigned long long *raddr) {
    /**
     * This is very likely to be incorrect/incomplete. I thought the base
     * address from `/proc/<pid>/maps` + the symbol address from `readelf` would
     * lead to the actual memory address, but on at least one system I tested on
     * this is not the case. On that system, working backwards from the address
     * printed in `gdb`, it seems the missing piece was the 'virtual address' of
     * the LOAD section in ELF headers. I suspect this may have to do with
     * address relocation and/or a feature called 'prelinking', but not sure.
     */
    char buf[128];
    unsigned long long start_addr;
    unsigned long long virt_addr;
    char *cmd_fmt = "grep ' %s$' /proc/%d/maps | head -n1";
    if (popen_read_line(buf, sizeof(buf), cmd_fmt, path, (int)pid) != 0) {
        fprintf(stderr, "get_php_base_addr: Failed to get start_addr\n");
        return 1;
    }
    start_addr = strtoull(buf, NULL, 16);
    cmd_fmt = "readelf -l %s | awk '/LOAD/{print $3; exit}'";
    if (popen_read_line(buf, sizeof(buf), cmd_fmt, path) != 0) {
        fprintf(stderr, "get_php_base_addr: Failed to get virt_addr\n");
        return 1;
    }
    virt_addr = strtoull(buf, NULL, 16);
    *raddr = start_addr - virt_addr;
    return 0;
}

static int get_executor_globals_offset(char *path, unsigned long long *raddr) {
    char buf[128];
    char *cmd_fmt = "readelf -s %s | grep ' executor_globals$' | awk 'NR==1{print $2}'";
    if (popen_read_line(buf, sizeof(buf), cmd_fmt, path) != 0) {
        fprintf(stderr, "get_php_executor_globals_offset: Failed\n");
        return 1;
    }
    *raddr = strtoull(buf, NULL, 16);
    return 0;
}

static int get_executor_globals_addr(unsigned long long *raddr) {
    char php_bin_path[128];
    unsigned long long base_addr;
    unsigned long long addr_offset;
    if (opt_executor_globals_addr != 0) {
        *raddr = opt_executor_globals_addr;
        return 0;
    }
    if (get_php_bin_path(opt_pid, php_bin_path) != 0) {
        return 1;
    }
    if (get_php_base_addr(opt_pid, php_bin_path, &base_addr) != 0) {
        return 1;
    }
    if (get_executor_globals_offset(php_bin_path, &addr_offset) != 0) {
        return 1;
    }
    *raddr = base_addr + addr_offset;
    return 0;
}

static int popen_read_line(char *buf, size_t buf_size, char *cmd_fmt, ...) {
    FILE *fp;
    char cmd[128];
    int buf_len;
    va_list cmd_args;
    va_start(cmd_args, cmd_fmt);
    vsprintf(cmd, cmd_fmt, cmd_args);
    va_end(cmd_args);
    if (!(fp = popen(cmd, "r"))) {
        perror("popen");
        return 1;
    }
    fgets(buf, buf_size-1, fp);
    pclose(fp);
    buf_len = strlen(buf);
    while (buf_len > 0 && buf[buf_len-1] == '\n') {
        --buf_len;
    }
    if (buf_len < 1) {
        fprintf(stderr, "popen_read_line: Expected strlen(buf)>0");
        return 1;
    }
    buf[buf_len] = '\0';
    return 0;
}

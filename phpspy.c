#define _GNU_SOURCE
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/uio.h>
#include <zend_API.h>

pid_t opt_pid = -1;
int opt_sleep_us = 10000;
unsigned long long opt_executor_globals_addr = 0;
int opt_max_stack_depth = -1;
char *opt_frame_delim = "\n";
char *opt_trace_delim = "";

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

    parse_opts(argc, argv);

    if (get_executor_globals_addr(&executor_globals_addr) != 0) {
        return 1;
    }

    while (1) {
        dump_trace(opt_pid, executor_globals_addr);
        usleep(opt_sleep_us);
    }

    return 0;
}

static void dump_trace(pid_t pid, unsigned long long executor_globals_addr) {
    char func[1024];
    char file[1024];
    int file_len;
    int func_len;
    int lineno;
    int depth;
    int wrote_trace;
    zend_execute_data* remote_execute_data;
    zend_executor_globals executor_globals;
    zend_execute_data execute_data;
    zend_op zop;
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
        memset(&zop, 0, sizeof(zop));

        try_copy_proc_mem("execute_data", remote_execute_data, &execute_data, sizeof(execute_data));
        try_copy_proc_mem("zfunc", execute_data.func, &zfunc, sizeof(zfunc));
        if (zfunc.common.function_name) {
            try_copy_proc_mem("function_name", zfunc.common.function_name, &zstring, sizeof(zstring));
            func_len = zstring.len;
            try_copy_proc_mem("function_name.val", ((char*)zfunc.common.function_name) + offsetof(zend_string, val), func, func_len);
        } else {
            func_len = snprintf(func, sizeof(func), "<main>");
        }
        if (zfunc.type == 2) {
            try_copy_proc_mem("zop", (void*)execute_data.opline, &zop, sizeof(zop));
            try_copy_proc_mem("filename", zfunc.op_array.filename, &zstring, sizeof(zstring));
            file_len = zstring.len;
            try_copy_proc_mem("filename.val", ((char*)zfunc.op_array.filename) + offsetof(zend_string, val), file, file_len);
            lineno = zop.lineno;
        } else {
            file_len = snprintf(file, sizeof(file), "<internal>");
            lineno = -1;
        }
        printf("%d %.*s %.*s:%d%s", depth, func_len, func, file_len, file, lineno, opt_frame_delim);
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
    char buf[128];
    char *cmd_fmt = "grep ' %s$' /proc/%d/maps | head -n1";
    if (popen_read_line(buf, sizeof(buf), cmd_fmt, path, (int)pid) != 0) {
        fprintf(stderr, "get_php_base_addr: Failed\n");
        return 1;
    }
    *raddr = strtoull(buf, NULL, 16);
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

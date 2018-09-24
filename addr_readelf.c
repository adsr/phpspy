#include "phpspy.h"
#ifdef USE_READELF

static int get_php_bin_path(pid_t pid, char *path);
static int get_php_base_addr(pid_t pid, char *path, uint64_t *raddr);
static int get_symbol_offset(char *path, const char *symbol, uint64_t *raddr);
static int popen_read_line(char *buf, size_t buf_size, char *cmd_fmt, ...);

static int get_symbol_addr(pid_t pid, const char *symbol, uint64_t *raddr) {
    char php_bin_path[128];
    uint64_t base_addr;
    uint64_t addr_offset;
    if (get_php_bin_path(pid, php_bin_path) != 0) {
        return 1;
    } else if (get_php_base_addr(pid, php_bin_path, &base_addr) != 0) {
        return 1;
    } else if (get_symbol_offset(php_bin_path, symbol, &addr_offset) != 0) {
        return 1;
    }
    *raddr = base_addr + addr_offset;
    return 0;
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

static int get_php_base_addr(pid_t pid, char *path, uint64_t *raddr) {
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
    uint64_t start_addr;
    uint64_t virt_addr;
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

static int get_symbol_offset(char *path, const char *symbol, uint64_t *raddr) {
    char buf[128];
    char *cmd_fmt = "readelf -s %s | grep ' %s$' | awk 'NR==1{print $2}'";
    if (popen_read_line(buf, sizeof(buf), cmd_fmt, path, symbol) != 0) {
        fprintf(stderr, "get_symbol_offset: Failed\n");
        return 1;
    }
    *raddr = strtoull(buf, NULL, 16);
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
    if (fgets(buf, buf_size-1, fp) == NULL) {
        fprintf(stderr, "popen_read_line: No stdout; cmd=%s\n", cmd);
        return 1;
    }
    pclose(fp);
    buf_len = strlen(buf);
    while (buf_len > 0 && buf[buf_len-1] == '\n') {
        --buf_len;
    }
    if (buf_len < 1) {
        fprintf(stderr, "popen_read_line: Expected strlen(buf)>0; cmd=%s\n", cmd);
        return 1;
    }
    buf[buf_len] = '\0';
    return 0;
}

#else
typedef int __no_readelf;
#endif

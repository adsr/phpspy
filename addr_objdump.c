#include "phpspy.h"
#include <assert.h>

#ifndef PHPSPY_WIN32
static int get_php_bin_path(pid_t pid, char *path_root, char *path);
static int get_php_base_addr(pid_t pid, char *path_root, char *path, uint64_t *raddr);
#else
static int get_php_info(pid_t pid, char *php_bin_path, char *php_dll_path, uint64_t *raddr);
#endif
static int get_symbol_offset(char *path_root, const char *symbol, uint64_t *raddr);
static int popen_read_line(char *buf, size_t buf_size, char *cmd_fmt, ...);

int shell_escape(const char *arg, char *buf, size_t buf_size, const char *what) {
    int rv = 0;
    char *const buf_end = buf + buf_size;
    assert(buf_size >= 1);
    *buf++ = '\'';

    while (*arg) {
        /* logic based on https://stackoverflow.com/a/3669819 */
        if (*arg == '\'') {
            if (buf_end - buf < 4) {
                rv = 1;
                goto shell_escape_end;
            }

            *buf++ = '\''; /* close quoting */
            *buf++ = '\\'; /* escape ... */
            *buf++ = '\''; /* ... a single quote */
            *buf++ = '\''; /* reopen quoting */
            arg++;
        } else {
            if (buf_end - buf < 1) {
                rv = 1;
                goto shell_escape_end;
            }

            *buf++ = *arg++;
        }
    }

    if (buf_end - buf < 2) {
        rv = 1;
        goto shell_escape_end;
    }

    *buf++ = '\'';
    *buf = '\0';

shell_escape_end:
    if (rv != 0) {
        log_error("shell_escape: Buffer too small to escape %s: %s\n", what, arg);
    }

    return rv;
}

int get_symbol_addr(addr_memo_t *memo, pid_t pid, const char *symbol, uint64_t *raddr) {
    char *php_bin_path, *php_bin_path_root;
    uint64_t *php_base_addr;
    uint64_t addr_offset;
    php_bin_path = memo->php_bin_path;
    php_bin_path_root = memo->php_bin_path_root;
    php_base_addr = &memo->php_base_addr;
#ifndef PHPSPY_WIN32
    if (*php_bin_path == '\0' && get_php_bin_path(pid, php_bin_path_root, php_bin_path) != 0) {
        return 1;
    } else if (*php_base_addr == 0 && get_php_base_addr(pid, php_bin_path_root, php_bin_path, php_base_addr) != 0) {
        return 1;
    } else if (get_symbol_offset(php_bin_path_root, symbol, &addr_offset) != 0) {
        return 1;
    }
#else
    char *php_dll_path = memo->php_dll_path;
    if (*php_bin_path == '\0' && get_php_info(pid, php_bin_path, php_dll_path, php_base_addr) != 0) {
        return 1;
    } else if (get_symbol_offset(php_dll_path, symbol, &addr_offset) != 0) {
        return 1;
    }
#endif
    *raddr = *php_base_addr + addr_offset;
    return 0;
}

#ifndef PHPSPY_WIN32

static int get_php_bin_path(pid_t pid, char *path_root, char *path) {
    char buf[PHPSPY_STR_SIZE];
    char libname[PHPSPY_STR_SIZE];
    if (shell_escape(opt_libname_awk_patt, libname, sizeof(libname), "opt_libname_awk_patt")) {
        return 1;
    }
    char *cmd_fmt = "awk -ve=1 -vp=%s '$0~p{print $NF; e=0; exit} END{exit e}' /proc/%d/maps"
        " || readlink /proc/%d/exe";
    if (popen_read_line(buf, sizeof(buf), cmd_fmt, libname, (int)pid, (int)pid) != 0) {
        log_error("get_php_bin_path: Failed\n");
        return 1;
    }
    if (snprintf(path_root, PHPSPY_STR_SIZE, "/proc/%d/root/%s", (int)pid, buf) > PHPSPY_STR_SIZE - 1) {
        log_error("get_php_bin_path: snprintf overflow\n");
        return 1;
    }
    if (access(path_root, F_OK) != 0) {
        snprintf(path_root, PHPSPY_STR_SIZE, "/proc/%d/exe", (int)pid);
    }
    strcpy(path, buf);
    return 0;
}

static int get_php_base_addr(pid_t pid, char *path_root, char *path, uint64_t *raddr) {
    /**
     * This is very likely to be incorrect/incomplete. I thought the base
     * address from `/proc/<pid>/maps` + the symbol address from `readelf` would
     * lead to the actual memory address, but on at least one system I tested on
     * this is not the case. On that system, working backwards from the address
     * printed in `gdb`, it seems the missing piece was the 'virtual address' of
     * the LOAD section in ELF headers. I suspect this may have to do with
     * address relocation and/or a feature called 'prelinking', but not sure.
     */
    char buf[PHPSPY_STR_SIZE];
    char arg_buf[PHPSPY_STR_SIZE];
    uint64_t start_addr;
    uint64_t virt_addr;
    char *cmd_fmt = "grep -m1 ' '%s\\$ /proc/%d/maps";
    if (shell_escape(path, arg_buf, sizeof(arg_buf), "path")) {
        return 1;
    }
    if (popen_read_line(buf, sizeof(buf), cmd_fmt, arg_buf, (int)pid) != 0) {
        log_error("get_php_base_addr: Failed to get start_addr\n");
        return 1;
    }
    start_addr = strtoull(buf, NULL, 16);
    if (shell_escape(path_root, arg_buf, sizeof(arg_buf), "path_root")) {
        return 1;
    }
    cmd_fmt = "objdump -p %s | awk '/LOAD/{print $5; exit}'";
    if (popen_read_line(buf, sizeof(buf), cmd_fmt, arg_buf) != 0) {
        log_error("get_php_base_addr: Failed to get virt_addr\n");
        return 1;
    }
    virt_addr = strtoull(buf, NULL, 16);
    *raddr = start_addr - virt_addr;
    return 0;
}

static int get_symbol_offset(char *path_root, const char *symbol, uint64_t *raddr) {
    char buf[PHPSPY_STR_SIZE];
    char arg_buf[PHPSPY_STR_SIZE];
    char *cmd_fmt = "objdump -Tt %s | awk '/ %s$/{print $1; exit}'";
    if (shell_escape(path_root, arg_buf, sizeof(arg_buf), "path_root")) {
        return 1;
    }
    if (popen_read_line(buf, sizeof(buf), cmd_fmt, arg_buf, symbol) != 0) {
        log_error("get_symbol_offset: Failed\n");
        return 1;
    }
    *raddr = strtoull(buf, NULL, 16);
    return 0;
}

#else

static int get_php_info(pid_t pid, char *php_bin_path, char *php_dll_path, uint64_t *raddr) {
    HANDLE hModuleSnap = INVALID_HANDLE_VALUE;
    MODULEENTRY32 me32;

    hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (hModuleSnap == INVALID_HANDLE_VALUE) {
        log_error("CreateToolhelp32Snapshot (of modules) failed. Error: %lu \n", GetLastError());
        return 1;
    }

    me32.dwSize = sizeof(MODULEENTRY32);

    if (!Module32First(hModuleSnap, &me32)) {
        printf("Module32First failed. Error: %lu \n", GetLastError());
        CloseHandle(hModuleSnap);  // Clean the snapshot object.
        return 1;
    }

    do {
        if (strncmp(me32.szModule, "php", 3) == 0 && strncmp(me32.szModule + 4, ".dll", 4) == 0) {
            strncpy(php_dll_path, me32.szExePath, PHPSPY_STR_SIZE - 1);
            *raddr = (uint64_t) me32.modBaseAddr;
        } else if (strncmp(me32.szModule, "php.exe", 7) == 0 || strncmp(me32.szModule, "php-cgi.exe", 11) == 0) {
            char *last_slash = strrchr(me32.szExePath, '\\');
            *last_slash = '\0';
            strncpy_s(php_bin_path, PHPSPY_STR_SIZE, me32.szExePath, sizeof(me32.szExePath) - 1);
            strncat(php_bin_path, "\\php.exe", 8);
        }
    } while (Module32Next(hModuleSnap, &me32));

    CloseHandle(hModuleSnap);

    return 0;
}

static int get_symbol_offset(char *path_root, const char *symbol, uint64_t *raddr) {
    char buf[PHPSPY_STR_SIZE];
    char arg_buf[PHPSPY_STR_SIZE];
    char hex_string[PHPSPY_STR_SIZE];
    char *cmd_fmt = "dumpbin /exports %s | findstr %s";

    if (popen_read_line(buf, sizeof(buf), cmd_fmt, path_root, symbol) != 0) {
        log_error("get_symbol_offset: Failed\n");
        return 1;
    }
    if (sscanf_s(buf, "%*s %*s %s", hex_string, sizeof(hex_string) - 1) == -1) {
        log_error("get_symbol_offset: sscanf_s Failed\n");
        return 1;
    }
    *raddr = strtoull(hex_string, NULL, 16);
    return 0;
}


#endif

static int popen_read_line(char *buf, size_t buf_size, char *cmd_fmt, ...) {
    FILE *fp;
    char cmd[PHPSPY_STR_SIZE];
    int buf_len;
    va_list cmd_args;
    va_start(cmd_args, cmd_fmt);
    if (vsnprintf(cmd, sizeof(cmd), cmd_fmt, cmd_args) >= (int)(sizeof(cmd) - 1)) {
        log_error("vsnprintf overflow\n");
        return 1;
    }
    va_end(cmd_args);
    if (!(fp = popen(cmd, "r"))) {
        perror("popen");
        return 1;
    }
    if (fgets(buf, buf_size-1, fp) == NULL) {
        log_error("popen_read_line: No stdout; cmd=%s\n", cmd);
        pclose(fp);
        return 1;
    }
    pclose(fp);
    buf_len = strlen(buf);
    while (buf_len > 0 && buf[buf_len-1] == '\n') {
        --buf_len;
    }
    if (buf_len < 1) {
        log_error("popen_read_line: Expected strlen(buf)>0; cmd=%s\n", cmd);
        return 1;
    }
    buf[buf_len] = '\0';
    return 0;
}

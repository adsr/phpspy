static char *debuginfo_path = NULL;
static const char *dwarf_lookup_symbol = NULL;
static const Dwfl_Callbacks proc_callbacks = {
    .find_elf = dwfl_linux_proc_find_elf,
    .find_debuginfo = dwfl_standard_find_debuginfo,
    .debuginfo_path = &debuginfo_path,
};

static int dwarf_module_callback(
    Dwfl_Module *mod,
    void **unused __attribute__((unused)),
    const char *name __attribute__((unused)),
    Dwarf_Addr start __attribute__((unused)),
    void *arg
) {
    unsigned long long *raddr = (unsigned long long *) arg;
    GElf_Sym sym;
    GElf_Addr value = 0;
    int i, n = dwfl_module_getsymtab(mod);

    for (i = 1; i < n; ++i) {
        const char *symbol_name = dwfl_module_getsym_info(mod, i, &sym, &value, NULL, NULL, NULL);
        if (symbol_name == NULL || symbol_name[0] == '\0') {
            continue;
        }
        switch (GELF_ST_TYPE(sym.st_info)) {
        case STT_SECTION:
        case STT_FILE:
        case STT_TLS:
            break;
        default:
            if (!strcmp(symbol_name, dwarf_lookup_symbol) && value != 0) {
                *raddr = value;
                return DWARF_CB_ABORT;
            }
            break;
        }
    }

    return DWARF_CB_OK;
}

static int get_symbol_addr(const char *symbol, unsigned long long *raddr) {
    Dwfl *dwfl = NULL;
    int ret = 0;

    dwarf_lookup_symbol = symbol;
    do {
        int err = 0;
        dwfl = dwfl_begin(&proc_callbacks);
        if (dwfl == NULL) {
            fprintf(stderr, "Error setting up DWARF reading. Details: %s\n", dwfl_errmsg(0));
            ret = 1;
            break;
        }

        err = dwfl_linux_proc_report(dwfl, opt_pid);
        if (err != 0) {
            fprintf(stderr, "Error reading from /proc. Details: %s\n", dwfl_errmsg(0));
            ret = 1;
            break;
        }

        if (dwfl_report_end(dwfl, NULL, NULL) != 0) {
            fprintf(stderr, "Error reading from /proc. Details: %s\n", dwfl_errmsg(0));
            ret = 1;
            break;
        }

        *raddr = 0;
        if (dwfl_getmodules(dwfl, dwarf_module_callback, raddr, 0) == -1) {
            fprintf(stderr, "Error reading DWARF modules. Details: %s\n", dwfl_errmsg(0));
            ret = 1;
            break;
        } else if (*raddr == 0) {
            fprintf(stderr, "Unable to find address of %s in the binary\n", symbol);
            ret = 1;
            break;
        }
    } while (0);
    dwarf_lookup_symbol = NULL;

    dwfl_end(dwfl);
    return ret;
}

#include <basic.h>
#ifdef PHPSPY_WIN32

void nanosleep(struct timespec *ts, void *null) {
    long ns = (ts->tv_sec * 1000000000UL) + (ts->tv_nsec * 1UL);
    Sleep(ns / 1000000L);
}

int asprintf(char **ret, char *fmt, ...) {
    va_list arg;
    int rv;

    va_start(arg, fmt);
    rv = vsnprintf(NULL, 0, fmt, arg);
    if (rv >= 0 && (*ret = malloc(rv + 1))) {
        vsnprintf(*ret, rv + 1, fmt, arg);
    }
    else {
        rv = rv < 0 ? rv : -1;
    }
    va_end(arg);

    return rv;
}

int gettimeofday(struct timeval *p, void *z)
{
    union {
        unsigned long long ns100; /*time since 1 Jan 1601 in 100ns units */
        FILETIME ft;
    } _now;

    if (p) {
        GetSystemTimeAsFileTime(&_now.ft);
        _now.ns100 -= 116444736000000000ull;
        _now.ns100 /= 10;
        p->tv_sec = _now.ns100 / 1000000ul;
        p->tv_usec = (long)(_now.ns100 % 1000000ul);
    }

    return 0;
}

int phpspy_thread_create(phpspy_thread_t *thread, phpspy_thread_attr_t *attr, phpspy_thread_func start_routine(void *), void* arg)
{
    *thread = (HANDLE*)_beginthreadex(attr, 0, start_routine, arg, 0, NULL);
    if (*thread) {
        return 0;
    } else {
        return (int)GetLastError();
    }
}

int phpspy_cond_timedwait(phpspy_cond_t *cond, phpspy_mutex_t *mutex, const struct timespec *abstime)
{
    DWORD milliseconds = INFINITE;
    if (abstime) {
        milliseconds = abstime->tv_sec * 1000 + abstime->tv_nsec / 1000000L;
    }
    if (SleepConditionVariableCS(cond, mutex, milliseconds)) {
        return 0;
    } else {
        return (int)GetLastError();
    }
}

#endif /* PHPSPY_WIN32 */

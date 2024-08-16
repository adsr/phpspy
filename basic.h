#ifndef BASIC_H
#define BASIC_H

#include <stdlib.h>
#include <stdarg.h>

#ifndef PHPSPY_WIN32
#define PHPSPY_PACK __attribute__((__packed__))
#ifndef gettid
#define gettid() syscall(SYS_gettid)
#endif
#endif


#ifdef PHPSPY_WIN32

#include <windows.h>
#include <process.h>
#include <time.h>
#include <tlhelp32.h>
#include <io.h>
#include <stdint.h>
#include <getopt_win.h>
#include <pcre2posix.h>


#define popen	_popen
#define pclose	_pclose
#define dup _dup
#define dup2 _dup2

#ifndef STDIN_FILENO
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif

#define PHPSPY_PACK
#define gettid() GetCurrentThreadId()
#define getpid() GetCurrentProcessId()

struct timezone
{
    int tz_minuteswest; /* of Greenwich */
    int tz_dsttime;     /* type of dst correction to apply */
};

typedef  uint32_t pid_t;
typedef __int64 ssize_t;

void nanosleep(struct timespec *ts, void *null);
int asprintf(char** ret, char* fmt, ...);
int gettimeofday(struct timeval* p, void* z);

#endif


/* Thread */
#ifndef PHPSPY_WIN32

typedef pthread_t phpspy_thread_t;
typedef pthread_attr_t phpspy_thread_attr_t;
typedef void *phpspy_thread_func;
#define PHPSPY_THREAD_RETNULL NULL;

typedef pthread_mutex_t phpspy_mutex_t;
typedef pthread_mutexattr_t phpspy_mutexattr_t;
typedef pthread_cond_t phpspy_cond_t;
typedef pthread_condattr_t  phpspy_condattr_t;

#define phpspy_thread_create  pthread_create
#define phpspy_thread_wait    pthread_join
#define phpspy_mutex_init     pthread_mutex_init
#define phpspy_mutex_destroy  pthread_mutex_destroy
#define phpspy_mutex_lock     pthread_mutex_lock
#define phpspy_mutex_unlock   pthread_mutex_unlock
#define phpspy_cond_init      pthread_cond_init
#define phpspy_cond_destroy   pthread_cond_destroy
#define phpspy_cond_timedwait pthread_cond_timedwait
#define phpspy_cond_signal    pthread_cond_signal
#define phpspy_cond_broadcast pthread_cond_broadcast

#endif


#ifdef PHPSPY_WIN32

typedef HANDLE phpspy_thread_t;
typedef SECURITY_ATTRIBUTES phpspy_thread_attr_t;
typedef unsigned __stdcall phpspy_thread_func;
#define PHPSPY_THREAD_RETNULL 0;

typedef CRITICAL_SECTION phpspy_mutex_t;
typedef void phpspy_mutexattr_t;
typedef CONDITION_VARIABLE phpspy_cond_t;
typedef void phpspy_condattr_t;

int phpspy_thread_create(phpspy_thread_t* thread, phpspy_thread_attr_t* attr, phpspy_thread_func start_routine(void*), void* arg);
#define phpspy_thread_wait(thread, value_ptr) WaitForSingleObject(thread, INFINITE)
#define phpspy_mutex_init(mutex, attr) InitializeCriticalSection(mutex)
#define phpspy_mutex_destroy(mutex) DeleteCriticalSection(mutex)
#define phpspy_mutex_lock(mutex) EnterCriticalSection(mutex)
#define phpspy_mutex_unlock(mutex) LeaveCriticalSection(mutex)
#define phpspy_cond_init(cond, attr) InitializeConditionVariable(cond)
#define phpspy_cond_destroy(cond) (void)cond
int phpspy_cond_timedwait(phpspy_cond_t* cond, phpspy_mutex_t* mutex, const struct timespec* abstime);
#define phpspy_cond_signal(cond) WakeConditionVariable(cond)
#define phpspy_cond_broadcast(cond) WakeAllConditionVariable(cond)

#endif
/* Thread */

#endif

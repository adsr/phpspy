#ifndef BASIC_H
#define BASIC_H

#ifndef WINDOWS

#define PHPSPY_PACK __attribute__((__packed__))
#ifndef gettid
#define gettid() syscall(SYS_gettid)
#endif

#endif


#ifdef WINDOWS

#include <windows.h>
#include <time.h>
#include <tlhelp32.h>
#include <io.h>
#include <stdint.h>
#include "vendor/getopt.h"


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
#define gettid() 1

struct timezone
{
	int tz_minuteswest; /* of Greenwich */
	int tz_dsttime;     /* type of dst correction to apply */
};

typedef  uint32_t pid_t;
typedef __int64 ssize_t;

void nanosleep(struct timespec *ts, void *null);

#endif

#endif

#include <basic.h>
#ifdef PHPSPY_WIN32

void nanosleep(struct timespec* ts, void *null) {
	long ns = (ts->tv_sec * 1000000000UL) + (ts->tv_nsec * 1UL);
	Sleep(ns / 1000000L);
}

#define FILETIME_1970 116444736000000000ull /* seconds between 1/1/1601 and 1/1/1970 */
#define HECTONANOSEC_PER_SEC 10000000ull

int getntptimeofday(struct timespec* tp, struct timezone* z)
{
	int res = 0;
	union {
		unsigned long long ns100; /*time since 1 Jan 1601 in 100ns units */
		FILETIME ft;
	}  _now;
	TIME_ZONE_INFORMATION  TimeZoneInformation;
	DWORD tzi;

	if (z != NULL)
	{
		if ((tzi = GetTimeZoneInformation(&TimeZoneInformation)) != TIME_ZONE_ID_INVALID) {
			z->tz_minuteswest = TimeZoneInformation.Bias;
			if (tzi == TIME_ZONE_ID_DAYLIGHT)
				z->tz_dsttime = 1;
			else
				z->tz_dsttime = 0;
		}
		else
		{
			z->tz_minuteswest = 0;
			z->tz_dsttime = 0;
		}
	}

	if (tp != NULL) {
		GetSystemTimeAsFileTime(&_now.ft);	 /* 100-nanoseconds since 1-1-1601 */
		/* The actual accuracy on XP seems to be 125,000 nanoseconds = 125 microseconds = 0.125 milliseconds */
		_now.ns100 -= FILETIME_1970;	/* 100 nano-seconds since 1-1-1970 */
		tp->tv_sec = _now.ns100 / HECTONANOSEC_PER_SEC;	/* seconds since 1-1-1970 */
		tp->tv_nsec = (long)(_now.ns100 % HECTONANOSEC_PER_SEC) * 100; /* nanoseconds */
	}
	return res;
}

int gettimeofday(struct timeval* p, void* z)
{
	struct timespec tp;

	if (getntptimeofday(&tp, (struct timezone*)z))
		return -1;
	p->tv_sec = tp.tv_sec;
	p->tv_usec = (tp.tv_nsec / 1000);
	return 0;
}

#endif

#include "util/time_utils.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

/* Get current time as ISO 8601 string (YYYY-MM-DDTHH:MM:SSZ) */
int cs_time_now_iso8601(char *buffer, size_t buffer_size) {
	time_t now;
	struct tm *tm_info;
	
	if (buffer == NULL || buffer_size < 21) {
		return -1;
	}
	
	now = time(NULL);
	if (now == (time_t)(-1)) {
		return -1;
	}
	
	tm_info = gmtime(&now);
	if (tm_info == NULL) {
		return -1;
	}
	
	if (strftime(buffer, buffer_size, "%Y-%m-%dT%H:%M:%SZ", tm_info) == 0) {
		return -1;
	}
	
	return 0;
}

/* Get current Unix timestamp (seconds since epoch) */
time_t cs_time_now_unix(void) {
	return time(NULL);
}

/* Get monotonic time in milliseconds (for measuring elapsed time) */
unsigned long long cs_time_monotonic_ms(void) {
#ifdef _WIN32
	LARGE_INTEGER freq, counter;
	double ms;
	
	if (!QueryPerformanceFrequency(&freq)) {
		return 0;
	}
	
	if (!QueryPerformanceCounter(&counter)) {
		return 0;
	}
	
	ms = ((double)counter.QuadPart / (double)freq.QuadPart) * 1000.0;
	return (unsigned long long)ms;
#else
	struct timespec ts;
	
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
		return 0;
	}
	
	return (unsigned long long)ts.tv_sec * 1000ULL + (unsigned long long)ts.tv_nsec / 1000000ULL;
#endif
}

/* Format Unix timestamp as ISO 8601 */
int cs_time_unix_to_iso8601(time_t unix_time, char *buffer, size_t buffer_size) {
	struct tm *tm_info;
	
	if (buffer == NULL || buffer_size < 21) {
		return -1;
	}
	
	tm_info = gmtime(&unix_time);
	if (tm_info == NULL) {
		return -1;
	}
	
	if (strftime(buffer, buffer_size, "%Y-%m-%dT%H:%M:%SZ", tm_info) == 0) {
		return -1;
	}
	
	return 0;
}

/* Parse ISO 8601 timestamp to Unix time */
time_t cs_time_iso8601_to_unix(const char *iso8601_str) {
	struct tm tm_time;
	time_t unix_time;
	int result;
	
	if (iso8601_str == NULL) {
		return (time_t)(-1);
	}
	
	memset(&tm_time, 0, sizeof(tm_time));
	
	/* Parse format: YYYY-MM-DDTHH:MM:SSZ */
	result = sscanf(iso8601_str, "%d-%d-%dT%d:%d:%dZ",
	                 &tm_time.tm_year, &tm_time.tm_mon, &tm_time.tm_mday,
	                 &tm_time.tm_hour, &tm_time.tm_min, &tm_time.tm_sec);
	
	if (result != 6) {
		return (time_t)(-1);
	}
	
	tm_time.tm_year -= 1900;
	tm_time.tm_mon -= 1;
	tm_time.tm_isdst = 0;
	
	unix_time = mktime(&tm_time);
	if (unix_time == (time_t)(-1)) {
		return (time_t)(-1);
	}
	
	return unix_time;
}

/* Sleep for milliseconds (cross-platform) */
void cs_time_sleep_ms(unsigned long milliseconds) {
#ifdef _WIN32
	Sleep(milliseconds);
#else
	usleep(milliseconds * 1000);
#endif
}

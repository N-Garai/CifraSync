#ifndef CIFRASYNC_UTIL_TIME_UTILS_H
#define CIFRASYNC_UTIL_TIME_UTILS_H

#include <time.h>

/* Get current time as ISO 8601 string (YYYY-MM-DDTHH:MM:SSZ) */
int cs_time_now_iso8601(char *buffer, size_t buffer_size);

/* Get current Unix timestamp (seconds since epoch) */
time_t cs_time_now_unix(void);

/* Get monotonic time in milliseconds (for measuring elapsed time) */
unsigned long long cs_time_monotonic_ms(void);

/* Format Unix timestamp as ISO 8601 */
int cs_time_unix_to_iso8601(time_t unix_time, char *buffer, size_t buffer_size);

/* Parse ISO 8601 timestamp to Unix time */
time_t cs_time_iso8601_to_unix(const char *iso8601_str);

/* Sleep for milliseconds (cross-platform) */
void cs_time_sleep_ms(unsigned long milliseconds);

#endif

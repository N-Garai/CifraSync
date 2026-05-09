#ifndef CIFRASYNC_COMMON_LOG_H
#define CIFRASYNC_COMMON_LOG_H

typedef enum cs_log_level {
	CS_LOG_LEVEL_DEBUG = 0,
	CS_LOG_LEVEL_INFO = 1,
	CS_LOG_LEVEL_WARN = 2,
	CS_LOG_LEVEL_ERROR = 3,
	CS_LOG_LEVEL_NONE = 4
} cs_log_level_t;

int cs_log_init(const char *file_path, cs_log_level_t min_level, int also_stderr);
void cs_log_shutdown(void);

void cs_log_set_level(cs_log_level_t min_level);
cs_log_level_t cs_log_get_level(void);

void cs_log_set_stderr_enabled(int enabled);
int cs_log_get_stderr_enabled(void);

const char *cs_log_level_to_string(cs_log_level_t level);
void cs_log_message(cs_log_level_t level, const char *fmt, ...);

#define CS_LOG_DEBUG(...) cs_log_message(CS_LOG_LEVEL_DEBUG, __VA_ARGS__)
#define CS_LOG_INFO(...) cs_log_message(CS_LOG_LEVEL_INFO, __VA_ARGS__)
#define CS_LOG_WARN(...) cs_log_message(CS_LOG_LEVEL_WARN, __VA_ARGS__)
#define CS_LOG_ERROR(...) cs_log_message(CS_LOG_LEVEL_ERROR, __VA_ARGS__)

#endif

#include "common/log.h"

#include "common/constants.h"
#include "util/time_utils.h"

#include <stdarg.h>
#include <stdio.h>

static FILE *g_log_file = NULL;
static int g_log_owns_file = 0;
static int g_log_stderr_enabled = 1;
static cs_log_level_t g_log_min_level = CS_LOG_LEVEL_INFO;

static void close_log_file_if_owned(void) {
	if (g_log_file != NULL && g_log_owns_file) {
		fclose(g_log_file);
	}
	g_log_file = NULL;
	g_log_owns_file = 0;
}

const char *cs_log_level_to_string(cs_log_level_t level) {
	switch (level) {
		case CS_LOG_LEVEL_DEBUG:
			return "DEBUG";
		case CS_LOG_LEVEL_INFO:
			return "INFO";
		case CS_LOG_LEVEL_WARN:
			return "WARN";
		case CS_LOG_LEVEL_ERROR:
			return "ERROR";
		default:
			return "UNKNOWN";
	}
}

int cs_log_init(const char *file_path, cs_log_level_t min_level, int also_stderr) {
	FILE *opened;

	close_log_file_if_owned();
	g_log_min_level = min_level;
	g_log_stderr_enabled = (also_stderr != 0) ? 1 : 0;

	if (file_path == NULL || file_path[0] == '\0') {
		return 0;
	}

	opened = fopen(file_path, "a");
	if (opened == NULL) {
		return -1;
	}

	g_log_file = opened;
	g_log_owns_file = 1;
	return 0;
}

void cs_log_shutdown(void) {
	close_log_file_if_owned();
	g_log_min_level = CS_LOG_LEVEL_INFO;
	g_log_stderr_enabled = 1;
}

void cs_log_set_level(cs_log_level_t min_level) {
	g_log_min_level = min_level;
}

cs_log_level_t cs_log_get_level(void) {
	return g_log_min_level;
}

void cs_log_set_stderr_enabled(int enabled) {
	g_log_stderr_enabled = (enabled != 0) ? 1 : 0;
}

int cs_log_get_stderr_enabled(void) {
	return g_log_stderr_enabled;
}

static void write_formatted(FILE *stream, const char *timestamp, const char *level, const char *fmt, va_list args) {
	fprintf(stream, "%s [%s] ", timestamp, level);
	vfprintf(stream, fmt, args);
	fputc('\n', stream);
	fflush(stream);
}

void cs_log_message(cs_log_level_t level, const char *fmt, ...) {
	char timestamp[CS_ISO8601_BUFSZ];
	const char *level_text;
	va_list args;

	if (fmt == NULL || level < g_log_min_level || level >= CS_LOG_LEVEL_NONE) {
		return;
	}

	if (cs_time_now_iso8601(timestamp, sizeof(timestamp)) != 0) {
		timestamp[0] = '1';
		timestamp[1] = '9';
		timestamp[2] = '7';
		timestamp[3] = '0';
		timestamp[4] = '-';
		timestamp[5] = '0';
		timestamp[6] = '1';
		timestamp[7] = '-';
		timestamp[8] = '0';
		timestamp[9] = '1';
		timestamp[10] = 'T';
		timestamp[11] = '0';
		timestamp[12] = '0';
		timestamp[13] = ':';
		timestamp[14] = '0';
		timestamp[15] = '0';
		timestamp[16] = ':';
		timestamp[17] = '0';
		timestamp[18] = '0';
		timestamp[19] = 'Z';
		timestamp[20] = '\0';
	}

	level_text = cs_log_level_to_string(level);

	va_start(args, fmt);
	if (g_log_file != NULL) {
		va_list file_args;
		va_copy(file_args, args);
		write_formatted(g_log_file, timestamp, level_text, fmt, file_args);
		va_end(file_args);
	}
	if (g_log_stderr_enabled) {
		va_list stderr_args;
		va_copy(stderr_args, args);
		write_formatted(stderr, timestamp, level_text, fmt, stderr_args);
		va_end(stderr_args);
	}
	va_end(args);
}

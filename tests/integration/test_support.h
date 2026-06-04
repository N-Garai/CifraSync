#ifndef CIFRASYNC_TESTS_INTEGRATION_TEST_SUPPORT_H
#define CIFRASYNC_TESTS_INTEGRATION_TEST_SUPPORT_H

#include "common/constants.h"
#include "common/path.h"
#include "core/engine.h"
#include "delta/hash.h"
#include "delta/manifest.h"
#include "fs/file_reader.h"
#include "net/client.h"
#include "net/protocol.h"
#include "net/server.h"
#include "storage/chunk_store.h"
#include "storage/repo.h"
#include "storage/snapshot_store.h"
#include "util/io_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <direct.h>
#include <process.h>
#include <windows.h>
#define cs_it_mkdir(path) _mkdir(path)
#else
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define cs_it_mkdir(path) mkdir(path, 0777)
#endif

#define CS_IT_PATH_CAP 4096U

static int cs_it_fail(const char *message) {
	fprintf(stderr, "[integration] %s\n", message != NULL ? message : "unknown failure");
	return -1;
}

static int cs_it_join_path(char *out, size_t out_size, const char *left, const char *right) {
	return cs_path_join(out, out_size, left, right);
}

static int cs_it_make_dirs(const char *path) {
	char normalized[CS_IT_PATH_CAP];
	size_t index;
	size_t start_index;
	size_t length;

	if (path == NULL) {
		return -1;
	}

	if (cs_path_normalize_copy(path, normalized, sizeof(normalized)) != 0) {
		return -1;
	}

	length = strlen(normalized);
	if (length == 0U) {
		return -1;
	}

#ifdef _WIN32
	start_index = (length >= 3U && normalized[1] == ':') ? 3U : 1U;
#else
	start_index = 1U;
#endif

	for (index = start_index; index < length; ++index) {
		if (normalized[index] == cs_path_separator()) {
			int status;
			normalized[index] = '\0';
			status = cs_it_mkdir(normalized);
			if (status != 0) {
#ifdef _WIN32
				if (GetLastError() != ERROR_ALREADY_EXISTS) {
					return -1;
				}
#else
				if (errno != EEXIST) {
					return -1;
				}
#endif
			}
			normalized[index] = cs_path_separator();
		}
	}

	if (cs_it_mkdir(normalized) != 0) {
		if (errno != EEXIST) {
			return -1;
		}
	}

	return 0;
}

static int cs_it_make_temp_root(const char *prefix, char *out, size_t out_size) {
#ifdef _WIN32
	char temp_path[CS_IT_PATH_CAP];
	unsigned long pid;
	unsigned long tick;
	DWORD written;

	if (prefix == NULL || out == NULL || out_size == 0U) {
		return -1;
	}

	written = GetTempPathA((DWORD)sizeof(temp_path), temp_path);
	if (written == 0U || written >= sizeof(temp_path)) {
		return -1;
	}

	pid = (unsigned long)GetCurrentProcessId();
	tick = (unsigned long)GetTickCount();
	if (snprintf(out, out_size, "%scifrasync_%s_%lu_%lu", temp_path, prefix, pid, tick) < 0) {
		return -1;
	}

	return cs_it_make_dirs(out);
#else
	char template[CS_IT_PATH_CAP];
	const char *tmp = getenv("TMPDIR");
	if (tmp == NULL) tmp = getenv("TMP");
	if (tmp == NULL) tmp = "/tmp";
	long pid = (long)getpid();
	unsigned long ticks = (unsigned long)time(NULL);
	if (snprintf(template, sizeof(template), "%s/cifrasync_%s_%ld_%lu", tmp, prefix, pid, ticks) < 0) return -1;
	if (cs_it_make_dirs(template) != 0) return -1;
	strncpy(out, template, out_size - 1);
	out[out_size - 1] = '\0';
	return 0;
#endif
}

static int cs_it_parent_dir(const char *path, char *out, size_t out_size) {
	char normalized[CS_IT_PATH_CAP];

	if (path == NULL || out == NULL || out_size == 0U) {
		return -1;
	}

	if (cs_path_normalize_copy(path, normalized, sizeof(normalized)) != 0) {
		return -1;
	}

	return cs_path_dirname(normalized, out, out_size);
}

static int cs_it_write_text_file(const char *path, const char *text) {
	char parent[CS_IT_PATH_CAP];
	FILE *fp;

	if (path == NULL || text == NULL) {
		return -1;
	}

	if (cs_it_parent_dir(path, parent, sizeof(parent)) == 0 && strcmp(parent, ".") != 0) {
		if (cs_it_make_dirs(parent) != 0) {
			return -1;
		}
	}

	fp = fopen(path, "wb");
	if (fp == NULL) {
		return -1;
	}

	if (fwrite(text, 1U, strlen(text), fp) != strlen(text)) {
		fclose(fp);
		return -1;
	}

	fclose(fp);
	return 0;
}

static int cs_it_write_relative_text_file(const char *root, const char *relative_path, const char *text) {
	char full_path[CS_IT_PATH_CAP];

	if (cs_it_join_path(full_path, sizeof(full_path), root, relative_path) != 0) {
		return -1;
	}

	return cs_it_write_text_file(full_path, text);
}

static int cs_it_path_exists(const char *path) {
#ifdef _WIN32
	DWORD attrs;

	if (path == NULL) {
		return 0;
	}

	attrs = GetFileAttributesA(path);
	return attrs != INVALID_FILE_ATTRIBUTES;
#else
	struct stat st;

	return path != NULL && stat(path, &st) == 0;
#endif
}

static int cs_it_remove_tree(const char *path) {
#ifdef _WIN32
	char pattern[CS_IT_PATH_CAP];
	WIN32_FIND_DATAA find_data;
	HANDLE handle;
	DWORD attrs;

	if (path == NULL || !cs_it_path_exists(path)) {
		return 0;
	}

	attrs = GetFileAttributesA(path);
	if (attrs == INVALID_FILE_ATTRIBUTES) {
		return 0;
	}
	if ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0U) {
		return DeleteFileA(path) ? 0 : -1;
	}

	if (snprintf(pattern, sizeof(pattern), "%s%c*", path, cs_path_separator()) < 0) {
		return -1;
	}

	handle = FindFirstFileA(pattern, &find_data);
	if (handle != INVALID_HANDLE_VALUE) {
		do {
			char child_path[CS_IT_PATH_CAP];
			if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
				continue;
			}
			if (cs_it_join_path(child_path, sizeof(child_path), path, find_data.cFileName) != 0) {
				FindClose(handle);
				return -1;
			}
			if (cs_it_remove_tree(child_path) != 0) {
				FindClose(handle);
				return -1;
			}
		} while (FindNextFileA(handle, &find_data) != 0);
		FindClose(handle);
	}

	return RemoveDirectoryA(path) ? 0 : -1;
#else
	DIR *dir;
	struct dirent *entry;
	struct stat st;

	if (path == NULL) {
		return 0;
	}

	if (lstat(path, &st) != 0) {
		return 0;
	}
	if (S_ISDIR(st.st_mode) == 0) {
		return remove(path);
	}

	dir = opendir(path);
	if (dir == NULL) {
		return -1;
	}

	while ((entry = readdir(dir)) != NULL) {
		char child_path[CS_IT_PATH_CAP];
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
			continue;
		}
		if (cs_it_join_path(child_path, sizeof(child_path), path, entry->d_name) != 0) {
			closedir(dir);
			return -1;
		}
		if (cs_it_remove_tree(child_path) != 0) {
			closedir(dir);
			return -1;
		}
	}

	closedir(dir);
	return rmdir(path);
#endif
}

static int cs_it_compare_files(const char *left_path, const char *right_path) {
	unsigned char *left_data = NULL;
	unsigned char *right_data = NULL;
	size_t left_size = 0U;
	size_t right_size = 0U;
	int status;

	status = cs_io_read_file(left_path, &left_data, &left_size);
	if (status != 0) {
		return -1;
	}
	status = cs_io_read_file(right_path, &right_data, &right_size);
	if (status != 0) {
		free(left_data);
		return -1;
	}

	if (left_size != right_size || memcmp(left_data, right_data, left_size) != 0) {
		free(left_data);
		free(right_data);
		return -1;
	}

	free(left_data);
	free(right_data);
	return 0;
}

static int cs_it_count_files_recursive_internal(const char *path, const char *suffix, unsigned long *count) {
#ifdef _WIN32
	char pattern[CS_IT_PATH_CAP];
	WIN32_FIND_DATAA find_data;
	HANDLE handle;
	DWORD attrs;

	attrs = GetFileAttributesA(path);
	if (attrs == INVALID_FILE_ATTRIBUTES) {
		return 0;
	}
	if ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0U) {
		if (suffix == NULL || strstr(path, suffix) != NULL) {
			(*count)++;
		}
		return 0;
	}

	if (snprintf(pattern, sizeof(pattern), "%s%c*", path, cs_path_separator()) < 0) {
		return -1;
	}
	handle = FindFirstFileA(pattern, &find_data);
	if (handle == INVALID_HANDLE_VALUE) {
		return 0;
	}

	do {
		char child_path[CS_IT_PATH_CAP];
		DWORD child_attrs;

		if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
			continue;
		}
		if (cs_it_join_path(child_path, sizeof(child_path), path, find_data.cFileName) != 0) {
			FindClose(handle);
			return -1;
		}
		child_attrs = find_data.dwFileAttributes;
		if ((child_attrs & FILE_ATTRIBUTE_DIRECTORY) != 0U) {
			if (cs_it_count_files_recursive_internal(child_path, suffix, count) != 0) {
				FindClose(handle);
				return -1;
			}
		} else if (suffix == NULL || strstr(find_data.cFileName, suffix) != NULL) {
			(*count)++;
		}
	} while (FindNextFileA(handle, &find_data) != 0);

	FindClose(handle);
	return 0;
#else
	DIR *dir;
	struct dirent *entry;
	struct stat st;

	if (lstat(path, &st) != 0) {
		return 0;
	}
	if (S_ISDIR(st.st_mode) == 0) {
		if (suffix == NULL || strstr(path, suffix) != NULL) {
			(*count)++;
		}
		return 0;
	}

	dir = opendir(path);
	if (dir == NULL) {
		return 0;
	}

	while ((entry = readdir(dir)) != NULL) {
		char child_path[CS_IT_PATH_CAP];
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
			continue;
		}
		if (cs_it_join_path(child_path, sizeof(child_path), path, entry->d_name) != 0) {
			closedir(dir);
			return -1;
		}
		if (cs_it_count_files_recursive_internal(child_path, suffix, count) != 0) {
			closedir(dir);
			return -1;
		}
	}

	closedir(dir);
	return 0;
#endif
}

static int cs_it_count_files_recursive(const char *path, const char *suffix, unsigned long *count) {
	if (count == NULL) {
		return -1;
	}
	return cs_it_count_files_recursive_internal(path, suffix, count);
}

static int cs_it_strip_suffix(const char *name, const char *suffix, char *out, size_t out_size) {
	size_t name_len;
	size_t suffix_len;

	if (name == NULL || suffix == NULL || out == NULL || out_size == 0U) {
		return -1;
	}

	name_len = strlen(name);
	suffix_len = strlen(suffix);
	if (name_len <= suffix_len || strcmp(name + name_len - suffix_len, suffix) != 0) {
		return -1;
	}

	if (name_len - suffix_len + 1U > out_size) {
		return -1;
	}

	memcpy(out, name, name_len - suffix_len);
	out[name_len - suffix_len] = '\0';
	return 0;
}

static int cs_it_find_latest_snapshot_stem(const char *repo_path, char *out, size_t out_size) {
	char snapshots_dir[CS_IT_PATH_CAP];
	char pattern[CS_IT_PATH_CAP];
	char best_name[CS_IT_PATH_CAP];
	FILETIME best_time;
	int found = 0;

	best_time.dwLowDateTime = 0U;
	best_time.dwHighDateTime = 0U;
	best_name[0] = '\0';

	if (cs_it_join_path(snapshots_dir, sizeof(snapshots_dir), repo_path, "snapshots") != 0) {
		return -1;
	}
	if (snprintf(pattern, sizeof(pattern), "%s%c*.snapshot", snapshots_dir, cs_path_separator()) < 0) {
		return -1;
	}

#ifdef _WIN32
	{
		WIN32_FIND_DATAA find_data;
		HANDLE handle = FindFirstFileA(pattern, &find_data);
		if (handle == INVALID_HANDLE_VALUE) {
			return -1;
		}

		do {
			if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) {
				continue;
			}
			if (!found || CompareFileTime(&find_data.ftLastWriteTime, &best_time) > 0) {
				FILETIME current_time = find_data.ftLastWriteTime;
				best_time = current_time;
				strncpy(best_name, find_data.cFileName, sizeof(best_name) - 1U);
				best_name[sizeof(best_name) - 1U] = '\0';
				found = 1;
			}
		} while (FindNextFileA(handle, &find_data) != 0);

		FindClose(handle);
	}
#else
	DIR *dir = opendir(snapshots_dir);
	if (dir == NULL) return -1;
	struct dirent *entry;
	time_t best_ts = 0;
	while ((entry = readdir(dir)) != NULL) {
		if (strstr(entry->d_name, ".snapshot") == NULL) continue;
		char full_path[CS_IT_PATH_CAP];
		struct stat st;
		cs_it_join_path(full_path, sizeof(full_path), snapshots_dir, entry->d_name);
		if (stat(full_path, &st) != 0) continue;
		if (!found || st.st_mtime > best_ts) {
			best_ts = st.st_mtime;
			strncpy(best_name, entry->d_name, sizeof(best_name) - 1U);
			best_name[sizeof(best_name) - 1U] = '\0';
			found = 1;
		}
	}
	closedir(dir);
#endif

	if (!found) {
		return -1;
	}

	return cs_it_strip_suffix(best_name, ".snapshot", out, out_size);
}

static int cs_it_snapshot_path_from_stem(const char *repo_path, const char *stem, const char *suffix, char *out, size_t out_size) {
	char snapshots_dir[CS_IT_PATH_CAP];
	char base[CS_IT_PATH_CAP];

	if (cs_it_join_path(snapshots_dir, sizeof(snapshots_dir), repo_path, "snapshots") != 0) {
		return -1;
	}
	if (cs_it_join_path(base, sizeof(base), snapshots_dir, stem) != 0) {
		return -1;
	}
	if (snprintf(out, out_size, "%s%s", base, suffix) < 0) {
		return -1;
	}
	return 0;
}

#endif
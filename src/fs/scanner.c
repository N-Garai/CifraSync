#include "fs/scanner.h"

#include "common/memory.h"
#include "common/path.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static int cs_fs_scan_visit_path(const char *path, const cs_fs_scan_options_t *options, cs_fs_scan_visit_fn visit, void *ctx);

void cs_fs_scan_options_default(cs_fs_scan_options_t *options) {
	if (options == NULL) {
		return;
	}
	options->recursive = 1;
	options->include_directories = 0;
}

#ifdef _WIN32
static int cs_fs_scan_recursive_windows(const char *path, const cs_fs_scan_options_t *options, cs_fs_scan_visit_fn visit, void *ctx) {
	char pattern[CS_PATH_CAP];
	WIN32_FIND_DATAA find_data;
	HANDLE handle;
	int is_directory;
	cs_fs_metadata_t metadata;

	if (cs_path_join(pattern, sizeof(pattern), path, "*") != 0) {
		return -1;
	}

	handle = FindFirstFileA(pattern, &find_data);
	if (handle == INVALID_HANDLE_VALUE) {
		return -1;
	}

	for (;;) {
		char child_path[CS_PATH_CAP];

		if (strcmp(find_data.cFileName, ".") != 0 && strcmp(find_data.cFileName, "..") != 0) {
			if (cs_path_join(child_path, sizeof(child_path), path, find_data.cFileName) != 0) {
				FindClose(handle);
				return -1;
			}

			is_directory = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
			cs_fs_metadata_init(&metadata);
			if (cs_fs_metadata_from_stat(&metadata, child_path,
				(unsigned long)find_data.dwFileAttributes,
				((unsigned long long)find_data.nFileSizeHigh << 32U) | (unsigned long long)find_data.nFileSizeLow,
				0,
				is_directory ? true : false,
				!is_directory,
				(find_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) != 0) {
				FindClose(handle);
				return -1;
			}

			if (!is_directory || (options != NULL && options->include_directories)) {
				if (visit(&metadata, ctx) != 0) {
					FindClose(handle);
					return -1;
				}
			}

			if (is_directory && (options == NULL || options->recursive)) {
				if (cs_fs_scan_recursive_windows(child_path, options, visit, ctx) != 0) {
					FindClose(handle);
					return -1;
				}
			}
		}

		if (!FindNextFileA(handle, &find_data)) {
			break;
		}
	}

	FindClose(handle);
	return 0;
}
#else
static int cs_fs_scan_recursive_posix(const char *path, const cs_fs_scan_options_t *options, cs_fs_scan_visit_fn visit, void *ctx) {
	DIR *dir;
	struct dirent *entry;
	struct stat st;
	int is_directory;
	cs_fs_metadata_t metadata;

	dir = opendir(path);
	if (dir == NULL) {
		return -1;
	}

	while ((entry = readdir(dir)) != NULL) {
		char child_path[CS_PATH_CAP];

		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
			continue;
		}

		if (cs_path_join(child_path, sizeof(child_path), path, entry->d_name) != 0) {
			closedir(dir);
			return -1;
		}
		if (stat(child_path, &st) != 0) {
			closedir(dir);
			return -1;
		}

		is_directory = S_ISDIR(st.st_mode) != 0;
		cs_fs_metadata_init(&metadata);
		if (cs_fs_metadata_from_stat(&metadata, child_path, (unsigned long)st.st_mode, (unsigned long long)st.st_size, st.st_mtime, is_directory ? true : false, S_ISREG(st.st_mode) != 0, S_ISLNK(st.st_mode) != 0) != 0) {
			closedir(dir);
			return -1;
		}

		if (!is_directory || (options != NULL && options->include_directories)) {
			if (visit(&metadata, ctx) != 0) {
				closedir(dir);
				return -1;
			}
		}

		if (is_directory && (options == NULL || options->recursive)) {
			if (cs_fs_scan_recursive_posix(child_path, options, visit, ctx) != 0) {
				closedir(dir);
				return -1;
			}
		}
	}

	closedir(dir);
	return 0;
}
#endif

static int cs_fs_scan_visit_path(const char *path, const cs_fs_scan_options_t *options, cs_fs_scan_visit_fn visit, void *ctx) {
	cs_fs_metadata_t metadata;

	if (path == NULL || visit == NULL) {
		return -1;
	}

	cs_fs_metadata_init(&metadata);
#ifdef _WIN32
	{
		DWORD attrs = GetFileAttributesA(path);
		if (attrs == INVALID_FILE_ATTRIBUTES) {
			return -1;
		}
		if (cs_fs_metadata_from_stat(&metadata, path, (unsigned long)attrs, 0ULL, 0, (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0, (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0, (attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0) != 0) {
			return -1;
		}
	}
#else
	{
		struct stat st;
		if (stat(path, &st) != 0) {
			return -1;
		}
		if (cs_fs_metadata_from_stat(&metadata, path, (unsigned long)st.st_mode, (unsigned long long)st.st_size, st.st_mtime, S_ISDIR(st.st_mode) != 0, S_ISREG(st.st_mode) != 0, S_ISLNK(st.st_mode) != 0) != 0) {
			return -1;
		}
	}
#endif

	if (!cs_fs_metadata_is_directory(&metadata) || (options != NULL && options->include_directories)) {
		if (visit(&metadata, ctx) != 0) {
			return -1;
		}
	}

	if (cs_fs_metadata_is_directory(&metadata) && (options == NULL || options->recursive)) {
	#ifdef _WIN32
		return cs_fs_scan_recursive_windows(path, options, visit, ctx);
	#else
		return cs_fs_scan_recursive_posix(path, options, visit, ctx);
	#endif
	}

	return 0;
}

int cs_fs_scan(const char *root_path, const cs_fs_scan_options_t *options, cs_fs_scan_visit_fn visit, void *ctx) {
	cs_fs_scan_options_t local_options;

	if (root_path == NULL || visit == NULL) {
		return -1;
	}

	if (options == NULL) {
		cs_fs_scan_options_default(&local_options);
		options = &local_options;
	}

	return cs_fs_scan_visit_path(root_path, options, visit, ctx);
}


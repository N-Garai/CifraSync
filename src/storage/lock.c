#include "storage/lock.h"
#include "common/path.h"
#include "common/memory.h"
#include "common/constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#endif

struct cs_lock {
	char lock_path[CS_PATH_CAP];
	cs_lock_mode_t mode;
#ifdef _WIN32
	HANDLE file_handle;
#else
	int fd;
#endif
};

#ifdef _WIN32
static int cs_lock_ensure_dir(const char *path) {
	if (CreateDirectoryA(path, NULL) != 0) {
		return 0;
	}
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		return 0;
	}
	return -1;
}
#else
static int cs_lock_ensure_dir(const char *path) {
	struct stat st;
	if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
		return 0;
	}
	if (mkdir(path, 0755) == 0) {
		return 0;
	}
	if (errno == EEXIST) {
		return 0;
	}
	return -1;
}
#endif

static int cs_lock_build_path(const char *repo_path, char *out, size_t out_size) {
	char locks_dir[CS_PATH_CAP];
	if (cs_path_join(locks_dir, sizeof(locks_dir), repo_path, "locks") != 0) {
		return -1;
	}
	if (cs_lock_ensure_dir(locks_dir) != 0) {
		return -1;
	}
	if (cs_path_join(out, out_size, locks_dir, "repo.lock") != 0) {
		return -1;
	}
	return 0;
}

int cs_lock_acquire(const char *repo_path, cs_lock_mode_t mode, cs_lock_t **out_lock) {
	cs_lock_t *lock;

	if (repo_path == NULL || out_lock == NULL) {
		return -1;
	}

	lock = (cs_lock_t *)cs_calloc(1, sizeof(*lock));
	if (lock == NULL) {
		return -1;
	}

	if (cs_lock_build_path(repo_path, lock->lock_path, sizeof(lock->lock_path)) != 0) {
		cs_free(lock);
		return -1;
	}

	lock->mode = mode;

#ifdef _WIN32
	{
		DWORD access = GENERIC_READ;
		DWORD share = FILE_SHARE_READ;
		DWORD flags = OPEN_ALWAYS;

		if (mode == CS_LOCK_EXCLUSIVE) {
			access = GENERIC_READ | GENERIC_WRITE;
			share = 0;
		}

		lock->file_handle = CreateFileA(lock->lock_path, access, share,
			NULL, flags, FILE_ATTRIBUTE_NORMAL, NULL);
		if (lock->file_handle == INVALID_HANDLE_VALUE) {
			cs_free(lock);
			return -1;
		}

		if (mode == CS_LOCK_EXCLUSIVE) {
			OVERLAPPED overlapped;
			memset(&overlapped, 0, sizeof(overlapped));
			if (LockFileEx(lock->file_handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
					0, 1, 0, &overlapped) == 0) {
				CloseHandle(lock->file_handle);
				cs_free(lock);
				return -1;
			}
		}
	}
#else
	{
		int flags = O_RDONLY | O_CREAT;
		mode_t create_mode = 0666;

		if (mode == CS_LOCK_EXCLUSIVE) {
			flags = O_RDWR | O_CREAT;
		}

		lock->fd = open(lock->lock_path, flags, create_mode);
		if (lock->fd < 0) {
			cs_free(lock);
			return -1;
		}

		{
			int op = (mode == CS_LOCK_EXCLUSIVE) ? LOCK_EX | LOCK_NB : LOCK_SH | LOCK_NB;
			if (flock(lock->fd, op) != 0) {
				close(lock->fd);
				cs_free(lock);
				return -1;
			}
		}
	}
#endif

	*out_lock = lock;
	return 0;
}

int cs_lock_try_acquire(const char *repo_path, cs_lock_mode_t mode, cs_lock_t **out_lock) {
	return cs_lock_acquire(repo_path, mode, out_lock);
}

void cs_lock_release(cs_lock_t *lock) {
	if (lock == NULL) {
		return;
	}

#ifdef _WIN32
	if (lock->file_handle != INVALID_HANDLE_VALUE) {
		if (lock->mode == CS_LOCK_EXCLUSIVE) {
			OVERLAPPED overlapped;
			memset(&overlapped, 0, sizeof(overlapped));
			UnlockFileEx(lock->file_handle, 0, 1, 0, &overlapped);
		}
		CloseHandle(lock->file_handle);
	}
#else
	if (lock->fd >= 0) {
		flock(lock->fd, LOCK_UN);
		close(lock->fd);
	}
#endif

	cs_memzero(lock, sizeof(*lock));
	cs_free(lock);
}

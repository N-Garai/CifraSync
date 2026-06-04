#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char **argv) {
	(void)argc; (void)argv;
	char exe_path[4096];
	char cmd[4608];

#ifdef _WIN32
	GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
	char *last_slash = strrchr(exe_path, '\\');
	if (last_slash) *last_slash = '\0';
	snprintf(cmd, sizeof(cmd),
		"cd /d \"%s\" && if not exist bin\\cifrasync.exe mingw32-make release >nul 2>&1 && bin\\cifrasync.exe",
		exe_path);
	system(cmd);
#else
	strncpy(exe_path, argv[0], sizeof(exe_path) - 1);
	char *last_slash = strrchr(exe_path, '/');
	if (last_slash) *last_slash = '\0';
	snprintf(cmd, sizeof(cmd),
		"cd \"%s\" && if [ ! -f bin/cifrasync ]; then make release >/dev/null 2>&1; fi && exec ./bin/cifrasync",
		exe_path);
	system(cmd);
#endif
	return 0;
}

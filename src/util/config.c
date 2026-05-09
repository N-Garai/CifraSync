#include "util/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

#define CS_CONFIG_DEFAULT_KEEP_SNAPSHOTS 7
#define CS_CONFIG_DEFAULT_PRUNE_DAYS 30
#define CS_CONFIG_LINE_MAX 512

/* Initialize config with default values */
int cs_config_init(cs_config_t *config) {
	if (config == NULL) {
		return -1;
	}
	memset(config, 0, sizeof(*config));
	config->keep_snapshots = CS_CONFIG_DEFAULT_KEEP_SNAPSHOTS;
	config->prune_days = CS_CONFIG_DEFAULT_PRUNE_DAYS;
	config->default_compress = malloc(16);
	if (config->default_compress == NULL) {
		return -1;
	}
	strcpy(config->default_compress, "zstd");
	
	config->default_encrypt = malloc(8);
	if (config->default_encrypt == NULL) {
		free(config->default_compress);
		return -1;
	}
	strcpy(config->default_encrypt, "yes");
	
	config->log_file = malloc(256);
	if (config->log_file == NULL) {
		free(config->default_compress);
		free(config->default_encrypt);
		return -1;
	}
#ifdef _WIN32
	strcpy(config->log_file, "cifrasync.log");
#else
	strcpy(config->log_file, "/var/log/cifrasync.log");
#endif
	
	return 0;
}

/* Load config from file */
int cs_config_load(cs_config_t *config, const char *config_path) {
	FILE *fp;
	char line[CS_CONFIG_LINE_MAX];
	char key[128];
	char value[256];
	
	if (config == NULL || config_path == NULL) {
		return -1;
	}
	
	/* Initialize with defaults first */
	if (cs_config_init(config) != 0) {
		return -1;
	}
	
	fp = fopen(config_path, "r");
	if (fp == NULL) {
		return -1;
	}
	
	while (fgets(line, sizeof(line), fp) != NULL) {
		/* Skip comments and empty lines */
		if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
			continue;
		}
		
		/* Parse key=value format */
		if (sscanf(line, "%127[^=]=%255s", key, value) == 2) {
			cs_config_set(config, key, value);
		}
	}
	
	fclose(fp);
	return 0;
}

/* Save config to file */
int cs_config_save(const cs_config_t *config, const char *config_path) {
	FILE *fp;
	
	if (config == NULL || config_path == NULL) {
		return -1;
	}
	
	fp = fopen(config_path, "w");
	if (fp == NULL) {
		return -1;
	}
	
	fprintf(fp, "# CifraSync Configuration File\n");
	fprintf(fp, "\n");
	
	if (config->default_repo != NULL) {
		fprintf(fp, "default_repo=%s\n", config->default_repo);
	}
	fprintf(fp, "default_compress=%s\n", config->default_compress);
	fprintf(fp, "default_encrypt=%s\n", config->default_encrypt);
	fprintf(fp, "log_file=%s\n", config->log_file);
	fprintf(fp, "keep_snapshots=%d\n", config->keep_snapshots);
	fprintf(fp, "prune_days=%d\n", config->prune_days);
	
	fclose(fp);
	return 0;
}

/* Get config value by key */
const char *cs_config_get(const cs_config_t *config, const char *key) {
	if (config == NULL || key == NULL) {
		return NULL;
	}
	
	if (strcmp(key, "default_repo") == 0) {
		return config->default_repo;
	}
	if (strcmp(key, "default_compress") == 0) {
		return config->default_compress;
	}
	if (strcmp(key, "default_encrypt") == 0) {
		return config->default_encrypt;
	}
	if (strcmp(key, "log_file") == 0) {
		return config->log_file;
	}
	
	return NULL;
}

/* Set config value by key */
int cs_config_set(cs_config_t *config, const char *key, const char *value) {
	if (config == NULL || key == NULL || value == NULL) {
		return -1;
	}
	
	if (strcmp(key, "default_repo") == 0) {
		if (config->default_repo != NULL) {
			free(config->default_repo);
		}
		config->default_repo = malloc(strlen(value) + 1);
		if (config->default_repo == NULL) {
			return -1;
		}
		strcpy(config->default_repo, value);
		return 0;
	}
	
	if (strcmp(key, "default_compress") == 0) {
		if (config->default_compress != NULL) {
			free(config->default_compress);
		}
		config->default_compress = malloc(strlen(value) + 1);
		if (config->default_compress == NULL) {
			return -1;
		}
		strcpy(config->default_compress, value);
		return 0;
	}
	
	if (strcmp(key, "default_encrypt") == 0) {
		if (config->default_encrypt != NULL) {
			free(config->default_encrypt);
		}
		config->default_encrypt = malloc(strlen(value) + 1);
		if (config->default_encrypt == NULL) {
			return -1;
		}
		strcpy(config->default_encrypt, value);
		return 0;
	}
	
	if (strcmp(key, "log_file") == 0) {
		if (config->log_file != NULL) {
			free(config->log_file);
		}
		config->log_file = malloc(strlen(value) + 1);
		if (config->log_file == NULL) {
			return -1;
		}
		strcpy(config->log_file, value);
		return 0;
	}
	
	if (strcmp(key, "keep_snapshots") == 0) {
		config->keep_snapshots = atoi(value);
		return 0;
	}
	
	if (strcmp(key, "prune_days") == 0) {
		config->prune_days = atoi(value);
		return 0;
	}
	
	return -1;
}

/* Free config resources */
void cs_config_free(cs_config_t *config) {
	if (config == NULL) {
		return;
	}
	
	if (config->default_repo != NULL) {
		free(config->default_repo);
		config->default_repo = NULL;
	}
	if (config->default_compress != NULL) {
		free(config->default_compress);
		config->default_compress = NULL;
	}
	if (config->default_encrypt != NULL) {
		free(config->default_encrypt);
		config->default_encrypt = NULL;
	}
	if (config->log_file != NULL) {
		free(config->log_file);
		config->log_file = NULL;
	}
	
	memset(config, 0, sizeof(*config));
}

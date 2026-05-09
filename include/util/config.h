#ifndef CIFRASYNC_UTIL_CONFIG_H
#define CIFRASYNC_UTIL_CONFIG_H

#include <stddef.h>

typedef struct {
	char *default_repo;
	char *default_compress;
	char *default_encrypt;
	char *log_file;
	int keep_snapshots;
	int prune_days;
} cs_config_t;

/* Initialize config with defaults */
int cs_config_init(cs_config_t *config);

/* Load config from file (Windows path supported) */
int cs_config_load(cs_config_t *config, const char *config_path);

/* Save config to file */
int cs_config_save(const cs_config_t *config, const char *config_path);

/* Get config value by key */
const char *cs_config_get(const cs_config_t *config, const char *key);

/* Set config value by key */
int cs_config_set(cs_config_t *config, const char *key, const char *value);

/* Free config resources */
void cs_config_free(cs_config_t *config);

#endif

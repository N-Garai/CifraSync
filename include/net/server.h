#ifndef CIFRASYNC_NET_SERVER_H
#define CIFRASYNC_NET_SERVER_H

#include <stddef.h>

#include "net/protocol.h"

typedef struct cs_net_server cs_net_server_t;

typedef int (*cs_net_server_handler_fn)(const cs_net_frame_t *request, cs_net_owned_frame_t *response, void *ctx);

int cs_net_server_run(const char *bind_host,
	unsigned short port,
	cs_net_server_handler_fn handler,
	void *ctx,
	int serve_once);

int cs_net_server_default_handler(const cs_net_frame_t *request, cs_net_owned_frame_t *response, void *ctx);

int cs_net_server_sync_handler(const cs_net_frame_t *request, cs_net_owned_frame_t *response, void *ctx);

const char *cs_net_server_last_error(void);

#endif


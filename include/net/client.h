#ifndef CIFRASYNC_NET_CLIENT_H
#define CIFRASYNC_NET_CLIENT_H

#include <stddef.h>

#include "net/protocol.h"

typedef struct cs_net_client {
	void *socket_handle;
	unsigned short port;
	char host[256];
	int connected;
} cs_net_client_t;

int cs_net_client_init(cs_net_client_t *client);
int cs_net_client_connect(cs_net_client_t *client, const char *host, unsigned short port);
void cs_net_client_close(cs_net_client_t *client);

int cs_net_client_send_frame(cs_net_client_t *client, const cs_net_frame_t *frame);
int cs_net_client_receive_frame(cs_net_client_t *client, cs_net_owned_frame_t *out_frame);

int cs_net_client_exchange_text(cs_net_client_t *client,
	cs_net_message_type_t request_type,
	const void *request_payload,
	size_t request_size,
	cs_net_owned_frame_t *out_reply);

int cs_net_client_send_manifest(cs_net_client_t *client, const char *manifest_text, cs_net_owned_frame_t *out_reply);

#endif


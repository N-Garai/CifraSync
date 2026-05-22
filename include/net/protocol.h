#ifndef CIFRASYNC_NET_PROTOCOL_H
#define CIFRASYNC_NET_PROTOCOL_H

#include <stddef.h>

typedef enum cs_net_message_type {
	CS_NET_MESSAGE_HELLO = 1,
	CS_NET_MESSAGE_MANIFEST = 2,
	CS_NET_MESSAGE_CHUNK = 3,
	CS_NET_MESSAGE_REQUEST = 4,
	CS_NET_MESSAGE_RESPONSE = 5,
	CS_NET_MESSAGE_ERROR = 6,
	CS_NET_MESSAGE_PING = 7,
	CS_NET_MESSAGE_PONG = 8,
	CS_NET_MESSAGE_BYE = 9
} cs_net_message_type_t;

typedef struct cs_net_frame {
	cs_net_message_type_t type;
	unsigned char *payload;
	size_t payload_size;
} cs_net_frame_t;

typedef struct cs_net_frame_header {
	unsigned char magic[4];
	unsigned char version;
	unsigned char type;
	unsigned char reserved[2];
	unsigned char payload_size[8];
} cs_net_frame_header_t;

typedef struct cs_net_owned_frame {
	cs_net_frame_t frame;
	unsigned char *storage;
} cs_net_owned_frame_t;

const char *cs_net_message_type_name(cs_net_message_type_t type);

int cs_net_frame_encode_alloc(cs_net_message_type_t type,
	const void *payload,
	size_t payload_size,
	unsigned char **out_buffer,
	size_t *out_buffer_size);

int cs_net_frame_decode(const unsigned char *buffer,
	size_t buffer_size,
	cs_net_owned_frame_t *out_frame);

void cs_net_owned_frame_reset(cs_net_owned_frame_t *frame);

size_t cs_net_frame_header_size(void);
size_t cs_net_frame_encoded_size(size_t payload_size);

#endif


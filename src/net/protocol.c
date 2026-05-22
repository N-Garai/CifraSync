#include "net/protocol.h"

#include "common/memory.h"

#include <string.h>

static const unsigned char cs_net_frame_magic[4] = {'C', 'S', 'N', 'F'};
static const unsigned char cs_net_frame_version = 1U;

static void cs_write_u64_be(unsigned char out[8], unsigned long long value) {
	for (size_t index = 0U; index < 8U; ++index) {
		out[7U - index] = (unsigned char)((value >> (index * 8U)) & 0xffU);
	}
}

static unsigned long long cs_read_u64_be(const unsigned char input[8]) {
	unsigned long long value = 0ULL;
	for (size_t index = 0U; index < 8U; ++index) {
		value = (value << 8U) | (unsigned long long)input[index];
	}
	return value;
}

const char *cs_net_message_type_name(cs_net_message_type_t type) {
	switch (type) {
	case CS_NET_MESSAGE_HELLO: return "HELLO";
	case CS_NET_MESSAGE_MANIFEST: return "MANIFEST";
	case CS_NET_MESSAGE_CHUNK: return "CHUNK";
	case CS_NET_MESSAGE_REQUEST: return "REQUEST";
	case CS_NET_MESSAGE_RESPONSE: return "RESPONSE";
	case CS_NET_MESSAGE_ERROR: return "ERROR";
	case CS_NET_MESSAGE_PING: return "PING";
	case CS_NET_MESSAGE_PONG: return "PONG";
	case CS_NET_MESSAGE_BYE: return "BYE";
	default: return "UNKNOWN";
	}
}

size_t cs_net_frame_header_size(void) {
	return sizeof(cs_net_frame_header_t);
}

size_t cs_net_frame_encoded_size(size_t payload_size) {
	return cs_net_frame_header_size() + payload_size;
}

int cs_net_frame_encode_alloc(cs_net_message_type_t type,
	const void *payload,
	size_t payload_size,
	unsigned char **out_buffer,
	size_t *out_buffer_size) {
	cs_net_frame_header_t header;
	unsigned char *buffer;

	if (out_buffer == NULL || out_buffer_size == NULL) {
		return -1;
	}
	if (payload == NULL && payload_size > 0U) {
		return -1;
	}

	buffer = (unsigned char *)cs_malloc(cs_net_frame_encoded_size(payload_size));
	if (buffer == NULL) {
		return -1;
	}

	memcpy(header.magic, cs_net_frame_magic, sizeof(header.magic));
	header.version = cs_net_frame_version;
	header.type = (unsigned char)type;
	header.reserved[0] = 0U;
	header.reserved[1] = 0U;
	cs_write_u64_be(header.payload_size, (unsigned long long)payload_size);

	memcpy(buffer, &header, sizeof(header));
	if (payload_size > 0U) {
		memcpy(buffer + sizeof(header), payload, payload_size);
	}

	*out_buffer = buffer;
	*out_buffer_size = cs_net_frame_encoded_size(payload_size);
	cs_memzero(&header, sizeof(header));
	return 0;
}

void cs_net_owned_frame_reset(cs_net_owned_frame_t *frame) {
	if (frame == NULL) {
		return;
	}
	if (frame->storage != NULL) {
		cs_free(frame->storage);
	}
	cs_memzero(frame, sizeof(*frame));
}

int cs_net_frame_decode(const unsigned char *buffer,
	size_t buffer_size,
	cs_net_owned_frame_t *out_frame) {
	const cs_net_frame_header_t *header;
	unsigned long long payload_size;

	if (buffer == NULL || out_frame == NULL || buffer_size < sizeof(cs_net_frame_header_t)) {
		return -1;
	}

	header = (const cs_net_frame_header_t *)buffer;
	if (memcmp(header->magic, cs_net_frame_magic, sizeof(header->magic)) != 0) {
		return -1;
	}
	if (header->version != cs_net_frame_version) {
		return -1;
	}

	payload_size = cs_read_u64_be(header->payload_size);
	if (buffer_size != sizeof(cs_net_frame_header_t) + (size_t)payload_size) {
		return -1;
	}

	cs_memzero(out_frame, sizeof(*out_frame));
	out_frame->storage = (unsigned char *)cs_memdup(buffer, buffer_size);
	if (out_frame->storage == NULL && buffer_size > 0U) {
		return -1;
	}
	out_frame->frame.type = (cs_net_message_type_t)header->type;
	out_frame->frame.payload_size = (size_t)payload_size;
	out_frame->frame.payload = out_frame->storage + sizeof(cs_net_frame_header_t);
	return 0;
}


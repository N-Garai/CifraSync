#include "net/client.h"

#include "common/memory.h"
#include "common/path.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET cs_socket_t;
#define cs_socket_invalid INVALID_SOCKET
#define cs_socket_close closesocket
#else
#include <errno.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int cs_socket_t;
#define cs_socket_invalid (-1)
#define cs_socket_close close
#endif

static int cs_socket_write_all(cs_socket_t socket_handle, const unsigned char *data, size_t size) {
	size_t sent = 0U;

	while (sent < size) {
		int result;
		if (data == NULL && size > 0U) {
			return -1;
		}
		result = send(socket_handle, (const char *)data + sent, (int)(size - sent), 0);
		if (result <= 0) {
			return -1;
		}
		sent += (size_t)result;
	}
	return 0;
}

static int cs_socket_read_all(cs_socket_t socket_handle, unsigned char *data, size_t size) {
	size_t received = 0U;

	while (received < size) {
		int result;
		if (data == NULL && size > 0U) {
			return -1;
		}
		result = recv(socket_handle, (char *)data + received, (int)(size - received), 0);
		if (result <= 0) {
			return -1;
		}
		received += (size_t)result;
	}
	return 0;
}

static int cs_client_socket_from_host(const char *host, unsigned short port, cs_socket_t *out_socket) {
	struct sockaddr_in address;
	struct hostent *host_entry;
	cs_socket_t socket_handle = cs_socket_invalid;
	unsigned long numeric_address;

	if (host == NULL || out_socket == NULL) {
		return -1;
	}

	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	numeric_address = inet_addr(host);
	if (numeric_address != INADDR_NONE) {
		address.sin_addr.s_addr = numeric_address;
	} else {
		host_entry = gethostbyname(host);
		if (host_entry == NULL || host_entry->h_addr_list == NULL || host_entry->h_addr_list[0] == NULL) {
			return -1;
		}
		memcpy(&address.sin_addr, host_entry->h_addr_list[0], (size_t)host_entry->h_length);
	}

	socket_handle = (cs_socket_t)socket(AF_INET, SOCK_STREAM, 0);
	if (socket_handle == cs_socket_invalid) {
		return -1;
	}

	if (connect(socket_handle, (struct sockaddr *)&address, (int)sizeof(address)) != 0) {
		cs_socket_close(socket_handle);
		return -1;
	}

	*out_socket = socket_handle;
	return 0;
}

int cs_net_client_init(cs_net_client_t *client) {
	if (client == NULL) {
		return -1;
	}
	cs_memzero(client, sizeof(*client));
	client->socket_handle = (void *)(intptr_t)cs_socket_invalid;
	return 0;
}

int cs_net_client_connect(cs_net_client_t *client, const char *host, unsigned short port) {
	cs_socket_t socket_handle;

	if (client == NULL || host == NULL) {
		return -1;
	}

#ifdef _WIN32
	{
		static int winsock_ready = 0;
		if (!winsock_ready) {
			WSADATA data;
			if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
				return -1;
			}
			winsock_ready = 1;
		}
	}
#endif

	if (cs_client_socket_from_host(host, port, &socket_handle) != 0) {
		return -1;
	}

	client->socket_handle = (void *)(intptr_t)socket_handle;
	client->port = port;
	strncpy(client->host, host, sizeof(client->host) - 1U);
	client->host[sizeof(client->host) - 1U] = '\0';
	client->connected = 1;
	return 0;
}

void cs_net_client_close(cs_net_client_t *client) {
	cs_socket_t socket_handle;

	if (client == NULL) {
		return;
	}

	socket_handle = (cs_socket_t)(intptr_t)client->socket_handle;
	if (client->connected && socket_handle != cs_socket_invalid) {
		cs_socket_close(socket_handle);
	}
	client->socket_handle = (void *)(intptr_t)cs_socket_invalid;
	client->connected = 0;
}

int cs_net_client_send_frame(cs_net_client_t *client, const cs_net_frame_t *frame) {
	unsigned char *encoded = NULL;
	size_t encoded_size = 0U;
	int status;

	if (client == NULL || frame == NULL || !client->connected) {
		return -1;
	}

	status = cs_net_frame_encode_alloc(frame->type, frame->payload, frame->payload_size, &encoded, &encoded_size);
	if (status != 0) {
		return status;
	}

	status = cs_socket_write_all((cs_socket_t)(intptr_t)client->socket_handle, encoded, encoded_size);
	cs_free(encoded);
	return status;
}

int cs_net_client_receive_frame(cs_net_client_t *client, cs_net_owned_frame_t *out_frame) {
	cs_net_frame_header_t header;
	unsigned long long payload_size;
	unsigned char *buffer;
	int status;

	if (client == NULL || out_frame == NULL || !client->connected) {
		return -1;
	}

	status = cs_socket_read_all((cs_socket_t)(intptr_t)client->socket_handle, (unsigned char *)&header, sizeof(header));
	if (status != 0) {
		return status;
	}

	payload_size = 0ULL;
	for (size_t index = 0U; index < 8U; ++index) {
		payload_size = (payload_size << 8U) | (unsigned long long)header.payload_size[index];
	}
	buffer = (unsigned char *)cs_malloc(sizeof(header) + (size_t)payload_size);
	if (buffer == NULL) {
		return -1;
	}

	memcpy(buffer, &header, sizeof(header));
	if (payload_size > 0U) {
		status = cs_socket_read_all((cs_socket_t)(intptr_t)client->socket_handle, buffer + sizeof(header), (size_t)payload_size);
		if (status != 0) {
			cs_free(buffer);
			return status;
		}
	}

	status = cs_net_frame_decode(buffer, sizeof(header) + (size_t)payload_size, out_frame);
	cs_free(buffer);
	return status;
}

int cs_net_client_exchange_text(cs_net_client_t *client,
	cs_net_message_type_t request_type,
	const void *request_payload,
	size_t request_size,
	cs_net_owned_frame_t *out_reply) {
	cs_net_frame_t request;
	int status;

	if (client == NULL || out_reply == NULL) {
		return -1;
	}

	request.type = request_type;
	request.payload = (unsigned char *)request_payload;
	request.payload_size = request_size;
	status = cs_net_client_send_frame(client, &request);
	if (status != 0) {
		return status;
	}
	return cs_net_client_receive_frame(client, out_reply);
}

int cs_net_client_send_manifest(cs_net_client_t *client, const char *manifest_text, cs_net_owned_frame_t *out_reply) {
	if (manifest_text == NULL) {
		return -1;
	}
	return cs_net_client_exchange_text(client, CS_NET_MESSAGE_MANIFEST, manifest_text, strlen(manifest_text), out_reply);
}


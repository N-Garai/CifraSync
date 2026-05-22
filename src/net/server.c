#include "net/server.h"

#include "common/memory.h"

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

static char cs_server_error_buffer[256];

static void cs_server_set_error(const char *message) {
	if (message == NULL) {
		cs_server_error_buffer[0] = '\0';
		return;
	}
	strncpy(cs_server_error_buffer, message, sizeof(cs_server_error_buffer) - 1U);
	cs_server_error_buffer[sizeof(cs_server_error_buffer) - 1U] = '\0';
}

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

static int cs_socket_send_frame(cs_socket_t socket_handle, const cs_net_frame_t *frame) {
	unsigned char *encoded = NULL;
	size_t encoded_size = 0U;
	int status;

	status = cs_net_frame_encode_alloc(frame->type, frame->payload, frame->payload_size, &encoded, &encoded_size);
	if (status != 0) {
		return status;
	}
	status = cs_socket_write_all(socket_handle, encoded, encoded_size);
	cs_free(encoded);
	return status;
}

static int cs_socket_receive_frame(cs_socket_t socket_handle, cs_net_owned_frame_t *out_frame) {
	cs_net_frame_header_t header;
	unsigned long long payload_size;
	unsigned char *buffer;
	int status;

	status = cs_socket_read_all(socket_handle, (unsigned char *)&header, sizeof(header));
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
		status = cs_socket_read_all(socket_handle, buffer + sizeof(header), (size_t)payload_size);
		if (status != 0) {
			cs_free(buffer);
			return status;
		}
	}

	status = cs_net_frame_decode(buffer, sizeof(header) + (size_t)payload_size, out_frame);
	cs_free(buffer);
	return status;
}

static int cs_server_resolve_bind_address(const char *bind_host, unsigned short port, struct sockaddr_in *out_address) {
	unsigned long numeric_address;
	struct hostent *host_entry;

	if (out_address == NULL) {
		return -1;
	}

	memset(out_address, 0, sizeof(*out_address));
	out_address->sin_family = AF_INET;
	out_address->sin_port = htons(port);

	if (bind_host == NULL || bind_host[0] == '\0') {
		out_address->sin_addr.s_addr = htonl(INADDR_ANY);
		return 0;
	}

	numeric_address = inet_addr(bind_host);
	if (numeric_address != INADDR_NONE) {
		out_address->sin_addr.s_addr = numeric_address;
		return 0;
	}

	host_entry = gethostbyname(bind_host);
	if (host_entry == NULL || host_entry->h_addr_list == NULL || host_entry->h_addr_list[0] == NULL) {
		return -1;
	}

	memcpy(&out_address->sin_addr, host_entry->h_addr_list[0], (size_t)host_entry->h_length);
	return 0;
}

int cs_net_server_default_handler(const cs_net_frame_t *request, cs_net_owned_frame_t *response, void *ctx) {
	char reply_text[256];
	const char *repo_path = (const char *)ctx;

	if (request == NULL || response == NULL) {
		return -1;
	}

	switch (request->type) {
	case CS_NET_MESSAGE_HELLO:
		snprintf(reply_text, sizeof(reply_text), "ACK HELLO%s%s", repo_path != NULL ? " repo=" : "", repo_path != NULL ? repo_path : "");
		break;
	case CS_NET_MESSAGE_MANIFEST:
		snprintf(reply_text, sizeof(reply_text), "ACK MANIFEST bytes=%lu", (unsigned long)request->payload_size);
		break;
	case CS_NET_MESSAGE_CHUNK:
		snprintf(reply_text, sizeof(reply_text), "ACK CHUNK bytes=%lu", (unsigned long)request->payload_size);
		break;
	case CS_NET_MESSAGE_PING:
		snprintf(reply_text, sizeof(reply_text), "PONG");
		break;
	case CS_NET_MESSAGE_BYE:
		snprintf(reply_text, sizeof(reply_text), "BYE");
		break;
	default:
		snprintf(reply_text, sizeof(reply_text), "ERROR unknown message %u", (unsigned)request->type);
		break;
	}

	response->frame.type = (request->type == CS_NET_MESSAGE_PING) ? CS_NET_MESSAGE_PONG : CS_NET_MESSAGE_RESPONSE;
	response->frame.payload_size = strlen(reply_text);
	response->storage = (unsigned char *)cs_memdup(reply_text, response->frame.payload_size);
	if (response->storage == NULL && response->frame.payload_size > 0U) {
		return -1;
	}
	response->frame.payload = response->storage;
	return 0;
}

int cs_net_server_run(const char *bind_host,
	unsigned short port,
	cs_net_server_handler_fn handler,
	void *ctx,
	int serve_once) {
	struct sockaddr_in bind_address;
	cs_socket_t listener = cs_socket_invalid;
	int status = -1;

	if (handler == NULL) {
		handler = cs_net_server_default_handler;
	}
	cs_server_set_error(NULL);

#ifdef _WIN32
	{
		static int winsock_ready = 0;
		if (!winsock_ready) {
			WSADATA data;
			if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
				cs_server_set_error("WSAStartup failed");
				return -1;
			}
			winsock_ready = 1;
		}
	}
#endif

	if (cs_server_resolve_bind_address(bind_host, port, &bind_address) != 0) {
		cs_server_set_error("bind address resolution failed");
		return -1;
	}

	{
		int enable = 1;
		listener = (cs_socket_t)socket(AF_INET, SOCK_STREAM, 0);
		if (listener == cs_socket_invalid) {
			cs_server_set_error("socket failed");
			return -1;
		}
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char *)&enable, (int)sizeof(enable));
		if (bind(listener, (struct sockaddr *)&bind_address, (int)sizeof(bind_address)) != 0 || listen(listener, 8) != 0) {
			cs_socket_close(listener);
			cs_server_set_error("bind/listen failed");
			return -1;
		}
	}

	for (;;) {
		struct sockaddr_storage peer;
		socklen_t peer_size = (socklen_t)sizeof(peer);
		cs_socket_t client = accept(listener, (struct sockaddr *)&peer, &peer_size);
		if (client == cs_socket_invalid) {
			cs_server_set_error("accept failed");
			break;
		}

		for (;;) {
			cs_net_owned_frame_t request_frame;
			cs_net_owned_frame_t response_frame;
			cs_net_frame_t request;
			if (cs_socket_receive_frame(client, &request_frame) != 0) {
				break;
			}

			request = request_frame.frame;
			if (handler(&request, &response_frame, ctx) != 0) {
				cs_net_owned_frame_reset(&request_frame);
				break;
			}

			if (cs_socket_send_frame(client, &response_frame.frame) != 0) {
				cs_net_owned_frame_reset(&response_frame);
				cs_net_owned_frame_reset(&request_frame);
				break;
			}

			if (request_frame.frame.type == CS_NET_MESSAGE_BYE || serve_once) {
				cs_net_owned_frame_reset(&response_frame);
				cs_net_owned_frame_reset(&request_frame);
				break;
			}

			cs_net_owned_frame_reset(&response_frame);
			cs_net_owned_frame_reset(&request_frame);
		}

		cs_socket_close(client);
		if (serve_once) {
			status = 0;
			break;
		}
	}

	cs_socket_close(listener);
	if (status != 0) {
		status = -1;
	}
	return status;
}

const char *cs_net_server_last_error(void) {
	return cs_server_error_buffer;
}


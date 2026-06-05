#include "net/server.h"

#include "common/constants.h"
#include "common/memory.h"
#include "common/path.h"
#include "storage/chunk_store.h"
#include "storage/snapshot_store.h"

#include <stdio.h>
#include <stdlib.h>
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

static cs_chunk_store_t *cs_sync_chunk_store = NULL;
static cs_snapshot_store_t *cs_sync_snapshot_store = NULL;
static char cs_sync_repo_path[CS_PATH_CAP] = "";

static int cs_sync_open_stores(const char *repo_path) {
	if (repo_path == NULL || repo_path[0] == '\0') {
		return -1;
	}
	strncpy(cs_sync_repo_path, repo_path, sizeof(cs_sync_repo_path) - 1U);
	cs_sync_repo_path[sizeof(cs_sync_repo_path) - 1U] = '\0';

	cs_sync_chunk_store = cs_chunk_store_open(repo_path);
	cs_sync_snapshot_store = cs_snapshot_store_open(repo_path);
	if (cs_sync_chunk_store == NULL || cs_sync_snapshot_store == NULL) {
		cs_chunk_store_close(cs_sync_chunk_store);
		cs_snapshot_store_close(cs_sync_snapshot_store);
		cs_sync_chunk_store = NULL;
		cs_sync_snapshot_store = NULL;
		return -1;
	}
	return 0;
}

static void cs_sync_close_stores(void) {
	cs_chunk_store_close(cs_sync_chunk_store);
	cs_snapshot_store_close(cs_sync_snapshot_store);
	cs_sync_chunk_store = NULL;
	cs_sync_snapshot_store = NULL;
	cs_sync_repo_path[0] = '\0';
}

static int cs_sync_store_manifest(const char *snapshot_id, const char *manifest_content, size_t content_len) {
	char manifest_path[CS_PATH_CAP];
	FILE *fp;

	if (cs_sync_repo_path[0] == '\0' || snapshot_id == NULL || manifest_content == NULL) {
		return -1;
	}

	{
		char snapshots_dir[CS_PATH_CAP];
		char safe_id[CS_PATH_CAP];
		size_t idx, dst;

		if (cs_path_join(snapshots_dir, sizeof(snapshots_dir), cs_sync_repo_path, "snapshots") != 0) {
			return -1;
		}

		for (idx = 0U, dst = 0U; snapshot_id[idx] != '\0' && dst + 1U < sizeof(safe_id); ++idx) {
			unsigned char ch = (unsigned char)snapshot_id[idx];
			if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '-' || ch == '_' || ch == '.') {
				safe_id[dst++] = (char)ch;
			} else {
				safe_id[dst++] = '-';
			}
		}
		safe_id[dst] = '\0';

		if (snprintf(manifest_path, sizeof(manifest_path), "%s%c%s.manifest", snapshots_dir, cs_path_separator(), safe_id) < 0) {
			return -1;
		}
	}

	fp = fopen(manifest_path, "wb");
	if (fp == NULL) {
		return -1;
	}

	if (fwrite(manifest_content, 1, content_len, fp) != content_len) {
		fclose(fp);
		return -1;
	}

	fclose(fp);
	return 0;
}

static int cs_sync_collect_missing_hashes(const char *manifest_text, size_t text_len, char ***out_hashes, size_t *out_count) {
	char *copy;
	char *line;
	char **hashes = NULL;
	size_t count = 0U;
	size_t capacity = 0U;

	if (manifest_text == NULL || out_hashes == NULL || out_count == NULL) {
		return -1;
	}

	copy = (char *)cs_malloc(text_len + 1U);
	if (copy == NULL) {
		return -1;
	}
	memcpy(copy, manifest_text, text_len);
	copy[text_len] = '\0';

	line = strtok(copy, "\n");
	while (line != NULL) {
		if (strncmp(line, "CHUNK\t", 6) == 0) {
			char *cursor = line + 6;
			char *field_end;

			field_end = strchr(cursor, '\t');
			if (field_end == NULL) continue;
			*field_end = '\0';
			cursor = field_end + 1;

			field_end = strchr(cursor, '\t');
			if (field_end == NULL) continue;
			*field_end = '\0';
			cursor = field_end + 1;

			if (strlen(cursor) == CS_HASH_HEX_LEN) {
				if (!cs_chunk_store_exists(cs_sync_chunk_store, cursor)) {
					if (count >= capacity) {
						size_t new_cap = capacity == 0U ? 128U : capacity * 2U;
						char **new_hashes = (char **)cs_realloc(hashes, new_cap * sizeof(char *));
						if (new_hashes == NULL) {
							for (size_t i = 0U; i < count; ++i) cs_free(hashes[i]);
							cs_free(hashes);
							cs_free(copy);
							return -1;
						}
						hashes = new_hashes;
						capacity = new_cap;
					}
					hashes[count] = cs_strdup(cursor);
					if (hashes[count] == NULL) {
						for (size_t i = 0U; i < count; ++i) cs_free(hashes[i]);
						cs_free(hashes);
						cs_free(copy);
						return -1;
					}
					count++;
				}
			}
		}
		line = strtok(NULL, "\n");
	}

	cs_free(copy);
	*out_hashes = hashes;
	*out_count = count;
	return 0;
}

static int cs_sync_parse_header_line(const char *line, char *out_id, size_t id_size,
	char *out_source, size_t src_size, time_t *out_ts,
	unsigned long *out_files, unsigned long long *out_size) {
	const char *cursor = line;

	if (line == NULL || out_id == NULL || out_source == NULL) {
		return -1;
	}

	while (cursor != NULL && *cursor != '\0') {
		const char *eq = strchr(cursor, '=');
		const char *tab = strchr(cursor, '\t');
		const char *next;

		if (tab != NULL) {
			next = tab + 1;
		} else {
			next = NULL;
		}

		if (eq != NULL && (tab == NULL || eq < tab)) {
			size_t key_len = (size_t)(eq - cursor);
			const char *val = eq + 1;
			size_t val_len;

			if (next != NULL) {
				val_len = (size_t)(next - 1 - val);
			} else {
				val_len = strlen(val);
			}

			if (key_len == 2 && strncmp(cursor, "id", 2) == 0) {
				if (val_len >= id_size) val_len = id_size - 1;
				memcpy(out_id, val, val_len);
				out_id[val_len] = '\0';
			} else if (key_len == 6 && strncmp(cursor, "source", 6) == 0) {
				if (val_len >= src_size) val_len = src_size - 1;
				memcpy(out_source, val, val_len);
				out_source[val_len] = '\0';
			} else if (key_len == 2 && strncmp(cursor, "ts", 2) == 0) {
				char tmp[32];
				if (val_len >= sizeof(tmp)) val_len = sizeof(tmp) - 1;
				memcpy(tmp, val, val_len);
				tmp[val_len] = '\0';
				if (out_ts != NULL) *out_ts = (time_t)atol(tmp);
			} else if (key_len == 5 && strncmp(cursor, "files", 5) == 0) {
				char tmp[32];
				if (val_len >= sizeof(tmp)) val_len = sizeof(tmp) - 1;
				memcpy(tmp, val, val_len);
				tmp[val_len] = '\0';
				if (out_files != NULL) *out_files = (unsigned long)atol(tmp);
			} else if (key_len == 4 && strncmp(cursor, "size", 4) == 0) {
				char tmp[32];
				if (val_len >= sizeof(tmp)) val_len = sizeof(tmp) - 1;
				memcpy(tmp, val, val_len);
				tmp[val_len] = '\0';
				if (out_size != NULL) *out_size = strtoull(tmp, NULL, 10);
			}
		}

		cursor = next;
	}

	return 0;
}

int cs_net_server_sync_handler(const cs_net_frame_t *request, cs_net_owned_frame_t *response, void *ctx) {
	const char *repo_path = (const char *)ctx;
	char reply_text[4096];

	if (request == NULL || response == NULL) {
		return -1;
	}

	switch (request->type) {
	case CS_NET_MESSAGE_HELLO: {
		if (repo_path != NULL && repo_path[0] != '\0' && cs_sync_repo_path[0] == '\0') {
			cs_sync_open_stores(repo_path);
		}
		snprintf(reply_text, sizeof(reply_text), "ACK HELLO repo=%s", repo_path != NULL ? repo_path : "");
		break;
	}
	case CS_NET_MESSAGE_MANIFEST: {
		const char *payload = (const char *)request->payload;
		size_t payload_size = request->payload_size;
		const char *first_nl;
		size_t header_len;
		char snap_path[CS_PATH_CAP] = "";
		char manifest_path[CS_PATH_CAP] = "";
		int wrote_snap = 0;
		int wrote_manifest = 0;

		if (cs_sync_repo_path[0] == '\0') {
			snprintf(reply_text, sizeof(reply_text), "ACK MANIFEST no-repo");
			break;
		}

		first_nl = (const char *)memchr(payload, '\n', payload_size);
		if (first_nl == NULL) {
			snprintf(reply_text, sizeof(reply_text), "ERROR invalid manifest format");
			break;
		}

		header_len = (size_t)(first_nl - payload);

		{
			char header_line[1024];
			char snapshot_id[64] = "";
			char source_path[CS_PATH_CAP] = "";
			time_t ts = 0;
			unsigned long files = 0UL;
			unsigned long long size = 0ULL;
			size_t header_copy = header_len;
			if (header_copy >= sizeof(header_line)) header_copy = sizeof(header_line) - 1;
			memcpy(header_line, payload, header_copy);
			header_line[header_copy] = '\0';

			cs_sync_parse_header_line(header_line, snapshot_id, sizeof(snapshot_id),
				source_path, sizeof(source_path), &ts, &files, &size);

			if (snapshot_id[0] == '\0') {
				snprintf(reply_text, sizeof(reply_text), "ERROR manifest missing snapshot id");
				break;
			}

			{
				char snap_dir[CS_PATH_CAP];
				char snap_name[CS_PATH_CAP];
				size_t idx, dst;
				FILE *fp;

				if (cs_path_join(snap_dir, sizeof(snap_dir), cs_sync_repo_path, "snapshots") != 0) {
					snprintf(reply_text, sizeof(reply_text), "ERROR failed to build snapshots dir");
					break;
				}

				for (idx = 0U, dst = 0U; snapshot_id[idx] != '\0' && dst + 1U < sizeof(snap_name); ++idx) {
					unsigned char ch = (unsigned char)snapshot_id[idx];
					if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '-' || ch == '_' || ch == '.') {
						snap_name[dst++] = (char)ch;
					} else {
						snap_name[dst++] = '-';
					}
				}
				snap_name[dst] = '\0';

				if (snprintf(snap_path, sizeof(snap_path), "%s%c%s.snapshot", snap_dir, cs_path_separator(), snap_name) < 0 ||
					snprintf(manifest_path, sizeof(manifest_path), "%s%c%s.manifest", snap_dir, cs_path_separator(), snap_name) < 0) {
					snprintf(reply_text, sizeof(reply_text), "ERROR failed to build artifact paths");
					break;
				}

				fp = fopen(snap_path, "wb");
				if (fp == NULL) {
					snprintf(reply_text, sizeof(reply_text), "ERROR failed to write snapshot %s", snapshot_id);
					break;
				}
				fprintf(fp, "id=%s\n", snapshot_id);
				fprintf(fp, "timestamp=%ld\n", (long)ts);
				fprintf(fp, "source_path=%s\n", source_path);
				fprintf(fp, "label=\n");
				fprintf(fp, "file_count=%lu\n", files);
				fprintf(fp, "size_bytes=%llu\n", size);
				fclose(fp);
				wrote_snap = 1;
			}

			if (cs_sync_store_manifest(snapshot_id, payload + header_len + 1U, payload_size - header_len - 1U) != 0) {
				snprintf(reply_text, sizeof(reply_text), "ERROR failed to store manifest for %s", snapshot_id);
				if (wrote_snap) remove(snap_path);
				break;
			}
			wrote_manifest = 1;

			{
				char **missing_hashes = NULL;
				size_t missing_count = 0U;

				if (cs_sync_collect_missing_hashes(payload + header_len + 1U, payload_size - header_len - 1U,
						&missing_hashes, &missing_count) != 0) {
					snprintf(reply_text, sizeof(reply_text), "ERROR failed to check chunks for %s", snapshot_id);
					if (wrote_snap) remove(snap_path);
					if (wrote_manifest) remove(manifest_path);
					break;
				}

				{
					size_t reply_cap = 128U;
					size_t reply_len = 0U;
					size_t written;
					char *reply_buf;

					if (missing_count > 0 && missing_count * (CS_HASH_HEX_LEN + 1U) > reply_cap) {
						reply_cap = missing_count * (CS_HASH_HEX_LEN + 1U) + 128U;
					}

					reply_buf = (char *)cs_malloc(reply_cap);
					if (reply_buf == NULL) {
						for (size_t i = 0U; i < missing_count; ++i) cs_free(missing_hashes[i]);
						cs_free(missing_hashes);
						if (wrote_snap) remove(snap_path);
						if (wrote_manifest) remove(manifest_path);
						snprintf(reply_text, sizeof(reply_text), "ERROR out of memory");
						break;
					}

					written = (size_t)snprintf(reply_buf, reply_cap, "ACK MANIFEST snapshot=%s missing=%lu", snapshot_id, (unsigned long)missing_count);
					if (written < reply_cap) reply_len = written;

					for (size_t i = 0U; i < missing_count; ++i) {
						size_t remaining = reply_cap - reply_len;
						written = (size_t)snprintf(reply_buf + reply_len, remaining, "\n%s", missing_hashes[i]);
						if (written < remaining) {
							reply_len += written;
						}
						cs_free(missing_hashes[i]);
					}
					cs_free(missing_hashes);

					response->frame.type = CS_NET_MESSAGE_RESPONSE;
					response->frame.payload_size = reply_len;
					response->storage = (unsigned char *)reply_buf;
					response->frame.payload = response->storage;
					return 0;
				}
			}
		}
	}
	case CS_NET_MESSAGE_CHUNK: {
		if (cs_sync_repo_path[0] == '\0') {
			snprintf(reply_text, sizeof(reply_text), "ACK CHUNK no-repo");
			break;
		}

		if (request->payload_size <= CS_HASH_HEX_LEN) {
			snprintf(reply_text, sizeof(reply_text), "ERROR chunk payload too small");
			break;
		}

		{
			char hash_hex[CS_HASH_HEX_BUFSZ];
			unsigned char *chunk_data;
			size_t chunk_size;

			memcpy(hash_hex, request->payload, CS_HASH_HEX_LEN);
			hash_hex[CS_HASH_HEX_LEN] = '\0';

			chunk_data = request->payload + CS_HASH_HEX_LEN;
			chunk_size = request->payload_size - CS_HASH_HEX_LEN;

			if (cs_chunk_store_put(cs_sync_chunk_store, hash_hex, chunk_data, chunk_size) != 0) {
				snprintf(reply_text, sizeof(reply_text), "ERROR failed to store chunk %s", hash_hex);
				break;
			}

			snprintf(reply_text, sizeof(reply_text), "ACK CHUNK %s", hash_hex);
		}
		break;
	}
	case CS_NET_MESSAGE_PING: {
		snprintf(reply_text, sizeof(reply_text), "PONG");
		break;
	}
	case CS_NET_MESSAGE_BYE: {
		cs_sync_close_stores();
		snprintf(reply_text, sizeof(reply_text), "BYE");
		break;
	}
	default: {
		snprintf(reply_text, sizeof(reply_text), "ERROR unknown message %u", (unsigned)request->type);
		break;
	}
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


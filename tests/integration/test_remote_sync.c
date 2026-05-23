#include "test_support.h"

#ifdef _WIN32
typedef struct cs_it_remote_ctx {
	unsigned short port;
	int result;
} cs_it_remote_ctx_t;

static unsigned __stdcall cs_it_remote_server_thread(void *param) {
	cs_it_remote_ctx_t *ctx = (cs_it_remote_ctx_t *)param;
	ctx->result = cs_net_server_run("127.0.0.1", ctx->port, NULL, NULL, 1);
	return 0U;
}

static int cs_it_remote_exchange_on_port(unsigned short port) {
	cs_it_remote_ctx_t ctx;
	cs_net_client_t client;
	cs_net_owned_frame_t reply;
	uintptr_t thread_handle;
	unsigned thread_id;
	int status;
	int attempt;

	ctx.port = port;
	ctx.result = -1;
	thread_handle = _beginthreadex(NULL, 0, cs_it_remote_server_thread, &ctx, 0, &thread_id);
	if (thread_handle == 0U) {
		return -1;
	}

	status = -1;
	cs_net_client_init(&client);
	memset(&reply, 0, sizeof(reply));
	for (attempt = 0; attempt < 40; ++attempt) {
		if (cs_net_client_connect(&client, "127.0.0.1", port) == 0) {
			break;
		}
		Sleep(50);
	}

	if (!client.connected) {
		WaitForSingleObject((HANDLE)thread_handle, INFINITE);
		CloseHandle((HANDLE)thread_handle);
		return -1;
	}

	if (cs_net_client_send_manifest(&client, "manifest:alpha=1\nmanifest:beta=2\n", &reply) != 0) {
		cs_net_client_close(&client);
		WaitForSingleObject((HANDLE)thread_handle, INFINITE);
		CloseHandle((HANDLE)thread_handle);
		return -1;
	}

	if (reply.frame.type != CS_NET_MESSAGE_RESPONSE || reply.frame.payload == NULL || reply.frame.payload_size == 0U) {
		cs_net_owned_frame_reset(&reply);
		cs_net_client_close(&client);
		WaitForSingleObject((HANDLE)thread_handle, INFINITE);
		CloseHandle((HANDLE)thread_handle);
		return -1;
	}

	if (strncmp((const char *)reply.frame.payload, "ACK MANIFEST", 12U) != 0) {
		cs_net_owned_frame_reset(&reply);
		cs_net_client_close(&client);
		WaitForSingleObject((HANDLE)thread_handle, INFINITE);
		CloseHandle((HANDLE)thread_handle);
		return -1;
	}

	status = 0;
	cs_net_owned_frame_reset(&reply);
	cs_net_client_close(&client);
	WaitForSingleObject((HANDLE)thread_handle, INFINITE);
	CloseHandle((HANDLE)thread_handle);
	return ctx.result == 0 ? status : -1;
}
#endif

int cs_integration_test_remote_sync(void) {
#ifdef _WIN32
	unsigned short base_port = (unsigned short)(50000U + (GetCurrentProcessId() % 1000U));
	unsigned short offset;

	for (offset = 0U; offset < 8U; ++offset) {
		if (cs_it_remote_exchange_on_port((unsigned short)(base_port + offset)) == 0) {
			return 0;
		}
	}
	return cs_it_fail("remote sync client/server exchange failed");
#else
	return cs_it_fail("remote sync test is Windows-focused in this workspace");
#endif
}
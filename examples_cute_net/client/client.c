#define CUTE_NET_IMPLEMENTATION
#include "../../cute_net.h"

#include <time.h>

// This can be whatever you want. It's a unique identifier for your game or application, and
// used merely to identify yourself within Cute's packets. It's more of a formality than anything.
uint64_t g_application_id = 0;

// These keys were generated by the below function `print_embedded_keygen`. Usually it's a good idea
// to distribute your keys to your servers in absolute secrecy, perhaps keeping them hidden in a file
// and never shared publicly. But, putting them here is fine for testing purposes.

// Embedded g_public_key
int g_public_key_sz = 32;
unsigned char g_public_key_data[32] = {
	0x4a,0xc5,0x56,0x47,0x30,0xbf,0xdc,0x22,0xc7,0x67,0x3b,0x23,0xc5,0x00,0x21,0x7e,
	0x19,0x3e,0xa4,0xed,0xbc,0x0f,0x87,0x98,0x80,0xac,0x89,0x82,0x30,0xe9,0x95,0x6c
};
// Embedded g_secret_key
int g_secret_key_sz = 64;
unsigned char g_secret_key_data[64] = {
	0x10,0xaa,0x98,0xe0,0x10,0x5a,0x3e,0x63,0xe5,0xdf,0xa4,0xb5,0x5d,0xf3,0x3c,0x0a,
	0x31,0x5d,0x6e,0x58,0x1e,0xb8,0x5b,0xa4,0x4e,0xa3,0xf8,0xe7,0x55,0x53,0xaf,0x7a,
	0x4a,0xc5,0x56,0x47,0x30,0xbf,0xdc,0x22,0xc7,0x67,0x3b,0x23,0xc5,0x00,0x21,0x7e,
	0x19,0x3e,0xa4,0xed,0xbc,0x0f,0x87,0x98,0x80,0xac,0x89,0x82,0x30,0xe9,0x95,0x6c
};

void print_embedded_symbol(const char* sym, void* data, int sz) {
	printf("// Embedded %s\n", sym);
	printf("int %s_sz = %d;\n", sym, sz);
	printf("unsigned char %s_data[%d] = {\n", sym, sz);

	const unsigned char* buf = (const unsigned char*)data;
	for (int i = 0; i < sz; ++i) {
		printf(
			i % 16 == 0 ? "\t0x%02x%s" : "0x%02x%s",
			buf[i],
			i == sz - 1 ? "" : (i + 1) % 16 == 0 ? ",\n" : ","
		);
	}
	printf("\n};\n");
}

void print_embedded_keygen() {
	cn_crypto_sign_public_t pk;
	cn_crypto_sign_secret_t sk;
	cn_crypto_sign_keygen(&pk, &sk);
	print_embedded_symbol("g_public_key", &pk, sizeof(pk));
	print_embedded_symbol("g_secret_key", &sk, sizeof(sk));
}

uint64_t unix_timestamp() {
	time_t ltime;
	time(&ltime);
	struct tm* timeinfo = gmtime(&ltime);;
	return (uint64_t)mktime(timeinfo);
}

cn_error_t make_test_connect_token(uint64_t unique_client_id, const char* address_and_port, uint8_t* connect_token_buffer) {
	cn_crypto_key_t client_to_server_key = cn_crypto_generate_key();
	cn_crypto_key_t server_to_client_key = cn_crypto_generate_key();
	uint64_t current_timestamp = unix_timestamp();
	uint64_t expiration_timestamp = current_timestamp + 60; // Token expires in one minute.
	uint32_t handshake_timeout = 5;
	const char* endpoints[] = {
		address_and_port,
	};

	uint8_t user_data[CN_CONNECT_TOKEN_USER_DATA_SIZE];
	memset(user_data, 0, sizeof(user_data));

	cn_error_t err = cn_generate_connect_token(
		g_application_id,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		sizeof(endpoints) / sizeof(endpoints[0]),
		endpoints,
		unique_client_id,
		user_data,
		(cn_crypto_sign_secret_t*)g_secret_key_data,
		connect_token_buffer
	);

	return err;
}

void panic(cn_error_t err) {
	printf("ERROR: %s\n", err.details);
	exit(-1);
}

// Get this header from here: https://github.com/RandyGaul/cute_headers/blob/master/cute_time.h
#define CUTE_TIME_IMPLEMENTATION
#include "../../cute_time.h"

int main(void) {
	// Must be unique for each different player in your game.
	uint64_t client_id = 5;

	const char* address_and_port = "127.0.0.1:5001";
	cn_endpoint_t endpoint;
	cn_endpoint_init(&endpoint, address_and_port);

	uint8_t connect_token[CN_CONNECT_TOKEN_SIZE];
	cn_client_t* client = cn_client_create(0, g_application_id, false, NULL);
	cn_error_t err = make_test_connect_token(client_id, address_and_port, connect_token);
	if (cn_is_error(err)) panic(err);
	err = cn_client_connect(client, connect_token);
	if (cn_is_error(err)) panic(err);
	printf("Attempting to connect to server on port %d.\n", (int)endpoint.port);

	float elapsed = 0;

	while (elapsed < 10.0f) {
		float dt = ct_time();
		uint64_t unix_time = unix_timestamp();
		elapsed += dt;

		cn_client_update(client, (double)dt, unix_time);

		if (cn_client_state_get(client) == CN_CLIENT_STATE_CONNECTED) {
			static bool notify = false;
			if (!notify) {
				notify = true;
				printf("Connected! Press ESC to gracefully disconnect.\n");
			}

			static float t = 0;
			t += dt;
			if (t > 2) {
				const char* data = "What's up over there, Mr. Server?";
				int size = (int)strlen(data) + 1;
				cn_client_send(client, data, size, false);
				t = 0;
			}
		} else if (cn_client_state_get(client) < 0) {
			printf("Client encountered an error: %s.\n", cn_client_state_string(cn_client_state_get(client)));
			exit(-1);
		}
	}

	cn_client_disconnect(client);
	cn_client_destroy(client);

	return 0;
}

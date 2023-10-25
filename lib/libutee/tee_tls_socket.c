#include <tee_internal_api.h>
#include <tee_isocket.h>
#include <tee_tlssocket.h>
#include <stdio.h>
#include <trace.h>

#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/debug.h>
#include <mbedtls/x509.h>

struct tls_socket_ctx {
	uint32_t protocolError;
	uint32_t state;
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_ssl_context ssl;
	mbedtls_ssl_config conf;
	mbedtls_x509_crt cacert;
	TEE_iSocket *base_sock;
	TEE_iSocketHandle *base_ctx;
};

static void my_debug(void __unused *ctx, int level, const char *file, int line,
		     const char *str)
{
	((void)level);
	DMSG_RAW("%s:%04d: %s", file, line, str);
}

int f_entropy(void *data, unsigned char *output, size_t size)
{
	((void)data);
	TEE_GenerateRandom(output, size);
	return 0;
}

static TEE_Result parse_credentials(TEE_tlsSocket_Setup *setup,
				    struct tls_socket_ctx *sock_ctx)
{
	TEE_tlsSocket_ServerPDC *server_creds;
	TEE_Result res;
	int i, j;

	if (!setup->credentials ||
	    setup->credentials->serverCredType != TEE_TLS_SERVER_CRED_CSC) {
		return TEE_ERROR_BAD_PARAMETERS;
	}

	server_creds = setup->credentials->serverCred;
	if (!server_creds || !server_creds->numTrustedCerts ||
	    !server_creds->trustedCertEncodings ||
	    !server_creds->trustedCerts) {
		return TEE_ERROR_BAD_PARAMETERS;
	}

	for (i = 0; i < server_creds->numTrustedCerts; i++) {
		uint32_t type = server_creds->trustedCertEncodings[i];
		if (type != 0x1 && type != 0x2) {
			return TEE_ERROR_BAD_PARAMETERS;
		}
	}

	for (i = 0; i < server_creds->numTrustedCerts; i++) {
		TEE_ObjectInfo cert_info = {};
		TEE_ObjectHandle cert_handle = server_creds->trustedCerts[i];
		const unsigned char *cert;
		uint32_t cert_len;

		TEE_GetObjectInfo1(server_creds->trustedCerts[i], &cert_info);
		cert = TEE_Malloc(cert_info.dataSize, TEE_MALLOC_FILL_ZERO);
		// Need to reset because a read will leave the data position at the EOS.
		TEE_SeekObjectData(cert_handle, 0, TEE_DATA_SEEK_SET);
		res = TEE_ReadObjectData(cert_handle, cert, cert_info.dataSize,
					 &cert_len);
		if (res != TEE_SUCCESS || cert_len != cert_info.dataSize) {
			EMSG("TEE_ReadObjectData failed 0x%08x, read %" PRIu32
			     " over %u",
			     res, cert_len, cert_info.dataSize);
			return res;
		}

		res = mbedtls_x509_crt_parse(&sock_ctx->cacert, cert, cert_len);
		if (res < 0) {
			EMSG(" failed\n  !  mbedtls_x509_crt_parse returned -0x%x\n\n",
			     (unsigned int)-res);
			return TEE_ERROR_BAD_FORMAT;
		}
	}

	return TEE_SUCCESS;
}

static int ssl_send(void *handle_param, const unsigned char *buf, size_t len)
{
	TEE_Result res;
	struct tls_socket_ctx *sock_ctx = handle_param;
	if ((res = sock_ctx->base_sock->send(*sock_ctx->base_ctx, buf, &len,
					     TEE_TIMEOUT_INFINITE)) ==
	    TEE_SUCCESS) {
		return len;
	}

	return (int)res;
}

static int ssl_recv(void *handle_param, unsigned char *buf, size_t len)
{
	TEE_Result res;
	struct tls_socket_ctx *sock_ctx = handle_param;

	if (len == 0) {
		return (int)TEE_ERROR_BAD_PARAMETERS;
	}

	switch ((res = sock_ctx->base_sock->recv(*sock_ctx->base_ctx, buf, &len,
						 TEE_TIMEOUT_INFINITE))) {
	case TEE_SUCCESS:
		return len;
	case TEE_ERROR_CANCEL:
		return MBEDTLS_ERR_SSL_WANT_READ;
	case TEE_ISOCKET_ERROR_TIMEOUT:
		return MBEDTLS_ERR_SSL_TIMEOUT;
	case TEE_ISOCKET_ERROR_REMOTE_CLOSED:
		return 0;
	case TEE_ERROR_COMMUNICATION:
	case TEE_ISOCKET_ERROR_PROTOCOL:
	case TEE_ISOCKET_WARNING_PROTOCOL:
	default:
		return (int)res;
	}

	return (int)res;
}

static int ssl_recv_timeout(void *handle_param, unsigned char *buf, size_t len,
			    uint32_t timeout)
{
	TEE_Result res;
	struct tls_socket_ctx *sock_ctx = handle_param;

	if (timeout == 0) {
		timeout = TEE_TIMEOUT_INFINITE;
	} else if (timeout == TEE_TIMEOUT_INFINITE) {
		timeout--; // Decrease to avoid infinite timeout.
	}

	switch ((res = sock_ctx->base_sock->recv(*sock_ctx->base_ctx, buf, &len,
						 timeout))) {
	case TEE_SUCCESS:
		return len;
	case TEE_ERROR_CANCEL:
		return MBEDTLS_ERR_SSL_WANT_READ;
	case TEE_ISOCKET_ERROR_TIMEOUT:
		return MBEDTLS_ERR_SSL_TIMEOUT;
	case TEE_ISOCKET_ERROR_REMOTE_CLOSED:
		return 0;
	case TEE_ERROR_COMMUNICATION:
	case TEE_ISOCKET_ERROR_PROTOCOL:
	case TEE_ISOCKET_WARNING_PROTOCOL:
	default:
		return (int)res;
	}
}

static TEE_Result tls_open(TEE_iSocketHandle *ctx, void *setup,
			   uint32_t *proto_error)
{
	TEE_Result res;
	struct tls_socket_ctx *sock_ctx;
	TEE_tlsSocket_Setup *tls_setup = setup;
	int i;

	if (!ctx || !tls_setup || !proto_error) {
		TEE_Panic(0);
	}
	*proto_error = TEE_SUCCESS;

	sock_ctx = TEE_Malloc(sizeof(*sock_ctx), TEE_MALLOC_FILL_ZERO);
	if (!sock_ctx)
		return TEE_ERROR_OUT_OF_MEMORY;

	if (!tls_setup->baseSocket || !tls_setup->baseContext) {
		return TEE_ERROR_BAD_PARAMETERS;
	}
	sock_ctx->base_sock = tls_setup->baseSocket;
	sock_ctx->base_ctx = tls_setup->baseContext;

	mbedtls_ctr_drbg_init(&sock_ctx->ctr_drbg);
	mbedtls_ssl_init(&sock_ctx->ssl);
	mbedtls_ssl_config_init(&sock_ctx->conf);
	mbedtls_x509_crt_init(&sock_ctx->cacert);
	mbedtls_entropy_init(&sock_ctx->entropy);

	mbedtls_ssl_conf_dbg(&sock_ctx->conf, my_debug, NULL);
	mbedtls_debug_set_threshold(1);

	if ((res = mbedtls_ctr_drbg_seed(
		     &sock_ctx->ctr_drbg, f_entropy, &sock_ctx->entropy,
		     tls_setup->persString, tls_setup->persStringLen)) != 0) {
		EMSG(" failed\n  ! mbedtls_ctr_drbg_seed returned %d\n", res);
		goto error;
	}

	if ((res = parse_credentials(tls_setup, sock_ctx)) != TEE_SUCCESS) {
		EMSG("Failed to parse certificates: %x", res);
		return res;
	}

	if ((res = mbedtls_ssl_config_defaults(
		     &sock_ctx->conf, MBEDTLS_SSL_IS_CLIENT,
		     MBEDTLS_SSL_TRANSPORT_STREAM,
		     MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
		EMSG(" failed\n  ! mbedtls_ssl_config_defaults returned %d\n\n",
		     res);
		goto error;
	}

	mbedtls_ssl_conf_rng(&sock_ctx->conf, mbedtls_ctr_drbg_random,
			     &sock_ctx->ctr_drbg);

	mbedtls_ssl_conf_ca_chain(&sock_ctx->conf, &sock_ctx->cacert, NULL);
	mbedtls_ssl_conf_authmode(&sock_ctx->conf, MBEDTLS_SSL_VERIFY_REQUIRED);

	if ((res = mbedtls_ssl_setup(&sock_ctx->ssl, &sock_ctx->conf)) != 0) {
		EMSG(" failed\n  ! mbedtls_ssl_setup returned %d\n\n", res);
		goto error;
	}

	if ((res = mbedtls_ssl_set_hostname(&sock_ctx->ssl,
					    tls_setup->serverName)) != 0) {
		EMSG(" failed\n  ! mbedtls_ssl_set_hostname returned -0x%x\n\n",
		     -res);
		goto error;
	}

	mbedtls_ssl_set_bio(&sock_ctx->ssl, sock_ctx, ssl_send, ssl_recv, NULL);

	while ((res = mbedtls_ssl_handshake(&sock_ctx->ssl)) != 0) {
		if (res != MBEDTLS_ERR_SSL_WANT_READ &&
		    res != MBEDTLS_ERR_SSL_WANT_WRITE) {
			EMSG(" failed\n  ! mbedtls_ssl_handshake returned -0x%x\n\n",
			     (unsigned int)-res);
			goto error;
		}
	}

	*ctx = (TEE_iSocketHandle)sock_ctx;

	return TEE_SUCCESS;

error:
	mbedtls_ssl_free(&sock_ctx->ssl);
	mbedtls_ssl_config_free(&sock_ctx->conf);
	mbedtls_ctr_drbg_free(&sock_ctx->ctr_drbg);
	mbedtls_entropy_free(&sock_ctx->entropy);

	return res;
}
static TEE_Result tls_close(TEE_iSocketHandle ctx)
{
	struct tls_socket_ctx *sock_ctx = (struct socket_ctx *)ctx;

	mbedtls_ssl_close_notify(&sock_ctx->ssl);
	mbedtls_ssl_free(&sock_ctx->ssl);
	mbedtls_ssl_config_free(&sock_ctx->conf);
	mbedtls_ctr_drbg_free(&sock_ctx->ctr_drbg);
	mbedtls_entropy_free(&sock_ctx->entropy);

	TEE_Free(sock_ctx);

	return TEE_SUCCESS;
}

static TEE_Result tls_send(TEE_iSocketHandle ctx, const void *buf,
			   uint32_t *length, uint32_t timeout)
{
	struct tls_socket_ctx *sock_ctx = (struct socket_ctx *)ctx;
	uint32_t sent = 0;
	int res;

	if (ctx == TEE_HANDLE_NULL || !buf || !length)
		TEE_Panic(0);

	while (sent < *length) { //timeout not reached
		res = mbedtls_ssl_write(&sock_ctx->ssl,
					(const unsigned char *)buf, *length);
		if (res < 0) {
			EMSG("Couldn't send data on socket: %x", res);
			/* TODO: Check for error message type and return
			 * correct error message 
			 */
			return res;
		}
		sent += res;
	}
	if (sent == 0) {
		return TEE_ISOCKET_ERROR_TIMEOUT;
	}

	*length = sent;
	return TEE_SUCCESS;
}

static TEE_Result tls_recv(TEE_iSocketHandle ctx, void *buf, uint32_t *length,
			   uint32_t timeout)
{
	TEE_Result res;
	struct tls_socket_ctx *sock_ctx = (struct tls_socket_ctx *)ctx;

	if (ctx == TEE_HANDLE_NULL || !length || (!buf && *length))
		TEE_Panic(0);

	if (*length == 0) {
		mbedtls_ssl_read(&sock_ctx->ssl, NULL, 0);
		*length = mbedtls_ssl_get_bytes_avail(&sock_ctx->ssl);
		return TEE_SUCCESS;
	}

	// mbedtls_ssl_conf_read_timeout(&sock_ctx->conf, timeout);
	res = mbedtls_ssl_read(&sock_ctx->ssl, (unsigned char *)buf, *length);
	if (res < 0) {
		EMSG("Couldn't get recv data: %x", res);
		/* TODO: Check for error message type and return
			* correct error message 
			*/
		return res;
	} else if (res == 0) {
		EMSG("Couldn't get recv data: %x", res);
		return TEE_ERROR_COMMUNICATION;
	}

	*length = res;
	return TEE_SUCCESS;
}

static uint32_t tls_error(TEE_iSocketHandle ctx)
{
	return 0;
}

static TEE_Result tls_ioctl(TEE_iSocketHandle ctx, uint32_t commandCode,
			    void *buf, uint32_t *length)
{
	return TEE_SUCCESS;
}

static TEE_iSocket tls_socket_instance = {
	.TEE_iSocketVersion = TEE_ISOCKET_VERSION,
	.protocolID = TEE_ISOCKET_PROTOCOLID_TLS,
	.open = tls_open,
	.close = tls_close,
	.send = tls_send,
	.recv = tls_recv,
	.error = tls_error,
	.ioctl = tls_ioctl,
};

TEE_iSocket *const TEE_tlsSocket = &tls_socket_instance;
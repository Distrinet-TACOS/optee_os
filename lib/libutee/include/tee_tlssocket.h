#ifndef __TEE_TLSSOCKET_H
#define __TEE_TLSSOCKET_H

#include <tee_isocket.h>
#include <__tee_tlssocket_defines.h>

typedef uint32_t TEE_tlsSocket_TlsVersion;
#define TEE_TLS_VERSION_ALL    0x00000000
#define TEE_TLS_VERSION_1v2    0x00000001
#define TEE_TLS_VERSION_PRE1v2 0x00000002
#define TEE_TLS_VERSION_1v3    0x00000004

#define TLS_NULL_WITH_NULL_NULL 0x0000, /* LIST TERMINATION */

/* Ciphersuites for TLS 1.2 and below */
typedef uint32_t *TEE_tlsSocket_CipherSuites_GroupA;
#define TEE_TLS_RSA_WITH_3DES_EDE_CBC_SHA	    0x0000000A /* [RFC5246] */
#define TEE_TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA	    0x00000013 /* [RFC5246] */
#define TEE_TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA	    0x00000016 /* [RFC5246] */
#define TEE_TLS_RSA_WITH_AES_128_CBC_SHA	    0x0000002F /* [RFC5246] */
#define TEE_TLS_DHE_DSS_WITH_AES_128_CBC_SHA	    0x00000032 /* [RFC5246] */
#define TEE_TLS_DHE_RSA_WITH_AES_128_CBC_SHA	    0x00000033 /* [RFC5246] */
#define TEE_TLS_RSA_WITH_AES_256_CBC_SHA	    0x00000035 /* [RFC5246] */
#define TEE_TLS_DHE_DSS_WITH_AES_256_CBC_SHA	    0x00000038 /* [RFC5246] */
#define TEE_TLS_DHE_RSA_WITH_AES_256_CBC_SHA	    0x00000039 /* [RFC5246] */
#define TEE_TLS_RSA_WITH_AES_128_CBC_SHA256	    0x0000003C /* [RFC5246] */
#define TEE_TLS_RSA_WITH_AES_256_CBC_SHA256	    0x0000003D /* [RFC5246] */
#define TEE_TLS_DHE_DSS_WITH_AES_128_CBC_SHA256	    0x00000040 /* [RFC5246] */
#define TEE_TLS_DHE_RSA_WITH_AES_128_CBC_SHA256	    0x00000067 /* [RFC5246] */
#define TEE_TLS_DHE_DSS_WITH_AES_256_CBC_SHA256	    0x0000006A /* [RFC5246] */
#define TEE_TLS_DHE_RSA_WITH_AES_256_CBC_SHA256	    0x0000006B /* [RFC5246] */
#define TEE_TLS_PSK_WITH_3DES_EDE_CBC_SHA	    0x0000008B /* [RFC4279] */
#define TEE_TLS_PSK_WITH_AES_128_CBC_SHA	    0x0000008C /* [RFC4279] */
#define TEE_TLS_PSK_WITH_AES_256_CBC_SHA	    0x0000008D /* [RFC4279] */
#define TEE_TLS_DHE_PSK_WITH_3DES_EDE_CBC_SHA	    0x0000008F /* [RFC4279] */
#define TEE_TLS_DHE_PSK_WITH_AES_128_CBC_SHA	    0x00000090 /* [RFC4279] */
#define TEE_TLS_DHE_PSK_WITH_AES_256_CBC_SHA	    0x00000091 /* [RFC4279] */
#define TEE_TLS_RSA_PSK_WITH_3DES_EDE_CBC_SHA	    0x00000093 /* [RFC4279] */
#define TEE_TLS_RSA_PSK_WITH_AES_128_CBC_SHA	    0x00000094 /* [RFC4279] */
#define TEE_TLS_RSA_PSK_WITH_AES_256_CBC_SHA	    0x00000095 /* [RFC4279] */
#define TEE_TLS_RSA_WITH_AES_128_GCM_SHA256	    0x0000009C /* [RFC5288] */
#define TEE_TLS_RSA_WITH_AES_256_GCM_SHA384	    0x0000009D /* [RFC5288] */
#define TEE_TLS_DHE_RSA_WITH_AES_128_GCM_SHA256	    0x0000009E /* [RFC5288] */
#define TEE_TLS_DHE_RSA_WITH_AES_256_GCM_SHA384	    0x0000009F /* [RFC5288] */
#define TEE_TLS_DHE_DSS_WITH_AES_128_GCM_SHA256	    0x000000A2 /* [RFC5288] */
#define TEE_TLS_DHE_DSS_WITH_AES_256_GCM_SHA384	    0x000000A3 /* [RFC5288] */
#define TEE_TLS_PSK_WITH_AES_128_GCM_SHA256	    0x000000A8 /* [RFC5487] */
#define TEE_TLS_PSK_WITH_AES_256_GCM_SHA384	    0x000000A9 /* [RFC5487] */
#define TEE_TLS_DHE_PSK_WITH_AES_128_GCM_SHA256	    0x000000AA /* [RFC5487] */
#define TEE_TLS_DHE_PSK_WITH_AES_256_GCM_SHA384	    0x000000AB /* [RFC5487] */
#define TEE_TLS_RSA_PSK_WITH_AES_128_GCM_SHA256	    0x000000AC /* [RFC5487] */
#define TEE_TLS_RSA_PSK_WITH_AES_256_GCM_SHA384	    0x000000AD /* [RFC5487] */
#define TEE_TLS_PSK_WITH_AES_128_CBC_SHA256	    0x000000AE /* [RFC5487] */
#define TEE_TLS_PSK_WITH_AES_256_CBC_SHA384	    0x000000AF /* [RFC5487] */
#define TEE_TLS_DHE_PSK_WITH_AES_128_CBC_SHA256	    0x000000B2 /* [RFC5487] */
#define TEE_TLS_DHE_PSK_WITH_AES_256_CBC_SHA384	    0x000000B3 /* [RFC5487] */
#define TEE_TLS_RSA_PSK_WITH_AES_128_CBC_SHA256	    0x000000B6 /* [RFC5487] */
#define TEE_TLS_RSA_PSK_WITH_AES_256_CBC_SHA384	    0x000000B7 /* [RFC5487] */
#define TEE_TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA   0x0000C008 /* [RFC4492] */
#define TEE_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA    0x0000C009 /* [RFC4492] */
#define TEE_TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA    0x0000C00A /* [RFC4492] */
#define TEE_TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA	    0x0000C012 /* [RFC4492] */
#define TEE_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA	    0x0000C013 /* [RFC4492] */
#define TEE_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA	    0x0000C014 /* [RFC4492] */
#define TEE_TLS_SRP_SHA_WITH_3DES_EDE_CBC_SHA	    0x0000C01A /* [RFC5054] */
#define TEE_TLS_SRP_SHA_RSA_WITH_3DES_EDE_CBC_SHA   0x0000C01B /* [RFC5054] */
#define TEE_TLS_SRP_SHA_DSS_WITH_3DES_EDE_CBC_SHA   0x0000C01C /* [RFC5054] */
#define TEE_TLS_SRP_SHA_WITH_AES_128_CBC_SHA	    0x0000C01D /* [RFC5054] */
#define TEE_TLS_SRP_SHA_RSA_WITH_AES_128_CBC_SHA    0x0000C01E /* [RFC5054] */
#define TEE_TLS_SRP_SHA_DSS_WITH_AES_128_CBC_SHA    0x0000C01F /* [RFC5054] */
#define TEE_TLS_SRP_SHA_WITH_AES_256_CBC_SHA	    0x0000C020 /* [RFC5054] */
#define TEE_TLS_SRP_SHA_RSA_WITH_AES_256_CBC_SHA    0x0000C021 /* [RFC5054] */
#define TEE_TLS_SRP_SHA_DSS_WITH_AES_256_CBC_SHA    0x0000C022 /* [RFC5054] */
#define TEE_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256 0x0000C023 /* [RFC5289] */
#define TEE_TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384 0x0000C024 /* [RFC5289] */
#define TEE_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256   0x0000C027 /* [RFC5289] */
#define TEE_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384   0x0000C028 /* [RFC5289] */
#define TEE_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 0x0000C02B /* [RFC5289] */
#define TEE_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 0x0000C02C /* [RFC5289] */
#define TEE_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256   0x0000C02F /* [RFC5289] */
#define TEE_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384   0x0000C030 /* [RFC5289] */
#define TEE_TLS_ECDHE_PSK_WITH_3DES_EDE_CBC_SHA	    0x0000C034 /* [RFC5489] */
#define TEE_TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA	    0x0000C035 /* [RFC5489] */
#define TEE_TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA	    0x0000C036 /* [RFC5489] */
#define TEE_TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256   0x0000C037 /* [RFC5489] */
#define TEE_TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA384   0x0000C038 /* [RFC5489] */
#define TEE_TLS_RSA_WITH_AES_128_CCM		    0x0000C09C /* [RFC6655] */
#define TEE_TLS_RSA_WITH_AES_256_CCM		    0x0000C09D /* [RFC6655] */
#define TEE_TLS_DHE_RSA_WITH_AES_128_CCM	    0x0000C09E /* [RFC6655] */
#define TEE_TLS_DHE_RSA_WITH_AES_256_CCM	    0x0000C09F /* [RFC6655] */
#define TEE_TLS_PSK_WITH_AES_128_CCM		    0x0000C0A4 /* [RFC6655] */
#define TEE_TLS_PSK_WITH_AES_256_CCM		    0x0000C0A5 /* [RFC6655] */
#define TEE_TLS_DHE_PSK_WITH_AES_128_CCM	    0x0000C0A6 /* [RFC6655] */
#define TEE_TLS_DHE_PSK_WITH_AES_256_CCM	    0x0000C0A7 /* [RFC6655] */

/* TLS 1.3 ciphersuites. */
typedef uint32_t *TEE_tlsSocket_CipherSuites_GroupB;
#define TEE_TLS_AES_128_GCM_SHA256	 0x00001301
#define TEE_TLS_AES_256_GCM_SHA384	 0x00001302
#define TEE_TLS_CHACHA20_POLY1305_SHA256 0x00001303
#define TEE_TLS_AES_128_CCM_SHA256	 0x00001304
#define TEE_TLS_AES_128_CCM_8_SHA256	 0x00001305

/* Signature algorithms. */
typedef uint32_t TEE_tlsSocket_SignatureScheme;
#define TEE_TLS_RSA_PKCS1_SHA256       0x00000401
#define TEE_TLS_RSA_PKCS1_SHA384       0x00000501
#define TEE_TLS_RSA_PKCS1_SHA512       0x00000601
#define TEE_TLS_ECDSA_SECP256R1_SHA256 0x00000403
#define TEE_TLS_ECDSA_SECP384R1_SHA384 0x00000503
#define TEE_TLS_ECDSA_SECP521R1_SHA512 0x00000603
#define TEE_TLS_RSA_PSS_RSAE_SHA256    0x00000804
#define TEE_TLS_RSA_PSS_RSAE_SHA384    0x00000805
#define TEE_TLS_RSA_PSS_RSAE_SHA512    0x00000806
#define TEE_TLS_ED25519		       0x00000807
#define TEE_TLS_ED448		       0x00000808
#define TEE_TLS_RSA_PSS_PSS_SHA256     0x00000809
#define TEE_TLS_RSA_PSS_PSS_SHA384     0x0000080A
#define TEE_TLS_RSA_PSS_PSS_SHA512     0x0000080B
#define TEE_TLS_RSA_PKCS_SHA1	       0x00000201
#define TEE_TLS_ECDSA_SHA1	       0x00000203

/* Key exchange groups used in TLS 1.3 */
typedef uint32_t TEE_tlsSocket_Tls13KeyExGroup;
#define TEE_TLS_KEYEX_GROUP_SECP256R1  0x00000017
#define TEE_TLS_KEYEX_GROUP_SECP384R1  0x00000018
#define TEE_TLS_KEYEX_GROUP_SECP521R1  0x00000019
#define TEE_TLS_KEYEX_GROUP_X25519     0x0000001D
#define TEE_TLS_KEYEX_GROUP_X4458      0x0000001E
#define TEE_TLS_KEYEX_GROUP_FFDHE_2048 0x00000100
#define TEE_TLS_KEYEX_GROUP_FFDHE_3072 0x00000101
#define TEE_TLS_KEYEX_GROUP_FFDHE_4096 0x00000102
#define TEE_TLS_KEYEX_GROUP_FFDHE_6144 0x00000103
#define TEE_TLS_KEYEX_GROUP_FFDHE_8192 0x00000104

typedef struct TEE_tlsSocket_PSK_Info_s {
	TEE_ObjectHandle pskKey;
	char *pskIdentity;
} TEE_tlsSocket_PSK_Info;

typedef struct TEE_tlsSocket_SRP_Info_s {
	char *srpPassword;
	char *srpIdentity;
} TEE_tlsSocket_SRP_Info;

typedef struct TEE_tlsSocket_ClientPDC_s {
	TEE_ObjectHandle privateKey;
	uint8_t *bulkCertChain;
	uint32_t bulkSize;
	uint32_t bulkEncoding;
} TEE_tlsSocket_ClientPDC;

typedef struct TEE_tlsSocket_ServerPDC_s {
	TEE_ObjectHandle publicKey;
	// The following fields were introduced in v1.1
	TEE_ObjectHandle *trustedCerts;
	uint32_t *trustedCertEncodings;
	uint32_t numTrustedCerts;
	uint32_t allowTAPersistentTimeCheck;
	uint8_t *certPins;
	uint32_t numCertPins;
	uint8_t *pubkeyPins;
	uint32_t numPubkeyPins;
} TEE_tlsSocket_ServerPDC;

typedef uint32_t TEE_tlsSocket_ClientCredentialType;
#define TEE_TLS_CLIENT_CRED_NONE 0x00000000
#define TEE_TLS_CLIENT_CRED_PDC	 0x00000001
#define TEE_TLS_CLIENT_CRED_CSC	 0x00000002

typedef uint32_t TEE_tlsSocket_ServerCredentialType;
#define TEE_TLS_SERVER_CRED_PDC	       0x00000000
#define TEE_TLS_SERVER_CRED_CSC	       0x00000001
#define TEE_TLS_SERVER_CRED_CERT_PIN   0x00000002
#define TEE_TLS_SERVER_CRED_PUBKEY_PIN 0x00000003

typedef struct TEE_tlsSocket_Credentials_s {
	TEE_tlsSocket_ServerCredentialType serverCredType;
	TEE_tlsSocket_ServerPDC *serverCred;
	TEE_tlsSocket_ClientCredentialType clientCredType;
	TEE_tlsSocket_ClientPDC *clientCred;
} TEE_tlsSocket_Credentials;

/*
* Struct for retrieving channel binding data
* using the ioctl functionality.
*/
typedef struct TEE_tlsSocket_CB_Data_s {
	uint32_t cb_data_size;
	uint8_t cb_data[];
} TEE_tlsSocket_CB_Data;

/*
* Struct for retrieving session information
* using the ioctl functionality.
*/
typedef struct TEE_tlsSocket_SessionInfo_s {
	uint8_t structVersion;
	TEE_tlsSocket_TlsVersion chosenVersion;
	uint32_t chosenCiphersuite;
	TEE_tlsSocket_SignatureScheme chosenSigAlg;
	TEE_tlsSocket_Tls13KeyExGroup chosenKeyExGroup;
	unsigned char *matchedServerName;
	uint32_t matchedServerNameLen;
	const uint8_t *validatedServerCertificate;
	uint32_t validatedServerCertificateLen;
	uint32_t usedServerAuthenticationMethod;
} TEE_tlsSocket_SessionInfo;

/* Structure for storing session tickets. */
typedef struct TEE_tlsSocket_SessionTicket_Info_s {
	uint8_t *encrypted_ticket;
	uint32_t encrypted_ticket_len;
	uint8_t *server_id;
	uint32_t server_id_len;
	uint8_t *session_params;
	uint32_t session_params_len;
	uint8_t caller_allocated;
	TEE_tlsSocket_PSK_Info psk;
} TEE_tlsSocket_SessionTicket_Info;

/* The TEE TLS setup struct */
typedef struct TEE_tlsSocket_Setup_s {
	TEE_tlsSocket_TlsVersion acceptServerVersion;
	TEE_tlsSocket_CipherSuites_GroupA *allowedCipherSuitesGroupA;
	TEE_tlsSocket_PSK_Info *PSKInfo;
	TEE_tlsSocket_SRP_Info *SRPInfo;
	TEE_tlsSocket_Credentials *credentials;
	TEE_iSocket *baseSocket;
	TEE_iSocketHandle *baseContext;
	// The following fields were introduced in v1.1
	TEE_tlsSocket_CipherSuites_GroupB *allowedCipherSuitesGroupB;
	TEE_tlsSocket_SignatureScheme *sigAlgs;
	uint32_t numSigAlgs;
	TEE_tlsSocket_SignatureScheme *certSigAlgs;
	uint32_t numCertSigAlgs;
	TEE_tlsSocket_Tls13KeyExGroup *tls13KeyExGroups;
	uint32_t numTls13KeyExGroups;
	uint32_t numTls13KeyShares;
	TEE_tlsSocket_SessionTicket_Info *sessionTickets;
	uint32_t sessionTicketsNumElements;
	uint32_t numStoredSessionTickets;
	unsigned char *serverName;
	uint32_t serverNameLen;
	uint8_t *serverCertChainBuf;
	uint32_t *serverCertChainBufLen;
	uint8_t storeServerCertChain;
	unsigned char **alpnProtocolIds;
	uint32_t *alpnProtocolIdLens;
	uint32_t numAlpnProtocolIds;

	/* MBEDTLS specific fields */
	const unsigned char *persString;
	uint32_t *persStringLen;
} TEE_tlsSocket_Setup;

/* declare the function pointer handle */
extern TEE_iSocket *const TEE_tlsSocket;

#endif /*__TEE_TLSSOCKET_H*/

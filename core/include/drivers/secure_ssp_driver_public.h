#ifndef SECURE_SSP_DRIVER_PUBLIC
#define SECURE_SSP_DRIVER_PUBLIC

#define PTA_NAME "Secure world ssp driver"
#define UUID1 0x661b512b
#define UUID2 0x53a3
#define UUID3 0x4cec
#define UUID4 0xa8
#define UUID5 0xfe
#define UUID6 0x48
#define UUID7 0x0c
#define UUID8 0x8a
#define UUID9 0x74
#define UUID10 0x05
#define UUID11 0xfe

enum con_split_command {
	REGISTER_ITR,
	UNREGISTER_ITR,
	WRITE_CHARS,
	UPDATE_BUFFER
};

#define BUFFER_SIZE 256

#endif /* SECURE_SSP_DRIVER_PUBLIC */

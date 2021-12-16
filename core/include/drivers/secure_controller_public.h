#ifndef SECURE_CONTROLLER_PUBLIC
#define SECURE_CONTROLLER_PUBLIC

// #define SEC_CONT_UUID
#define UUID1 0xf84ba0a6
#define UUID2 0x1b24
#define UUID3 0x4ef0
#define UUID4 0x91
#define UUID5 0xdd
#define UUID6 0xe7
#define UUID7 0xfa
#define UUID8 0x37
#define UUID9 0x60
#define UUID10 0xf8
#define UUID11 0x7f

enum con_split_command {
	GET_NOTIF_VALUE,
	GET_NOTIFYING_UUID,
};

#define ALLOCATED_UUIDS 5

#endif // SECURE_CONTROLLER_PUBLIC
#include <drivers/serial.h>
#include <kernel/interrupt.h>

void register_serial_chip_con_split(struct serial_chip *chip);

#define PTA_NAME "Secure world split driver"
#define PTA_CONSOLE_SPLIT_UUID { 0x661b512b, 0x53a3, 0x4cec, \
                { 0xa8, 0xfe, 0x48, 0x0c, 0x8a, 0x74, 0x05, 0xfe} }

enum con_split_command { READ_CHAR, REGISTER_ITR, UNREGISTER_ITR };
#include <drivers/serial.h>

static struct serial_chip *serial_chip;
void register_serial_chip(struct serial_chip *chip);
#ifndef LINUX_REBOOT_H
#define LINUX_REBOOT_H

#include <types_ext.h>

void set_nsec_entry_reboot(unsigned long nsec_entry, unsigned long dt_addr,
			   unsigned long args);

uint32_t change_to_normal_world(void);
void change_to_secure_world(uint32_t cpsr);
void reset_linux(unsigned long dt_addr, unsigned long args);

#endif /* LINUX_REBOOT_H */
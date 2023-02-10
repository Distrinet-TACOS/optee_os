#ifndef LINUX_REBOOT_H
#define LINUX_REBOOT_H

#include <types_ext.h>
#include <tee_internal_api.h>
#include <kernel/interrupt.h>

void set_nsec_entry_reboot(unsigned long nsec_entry, unsigned long dt_addr,
			   unsigned long args);

TEE_Result update_image(void **img, size_t *size);
void restart_normal_world(void *img, size_t size);

void reset_linux(unsigned long dt_addr, unsigned long args);

#endif /* LINUX_REBOOT_H */
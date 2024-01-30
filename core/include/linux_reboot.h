#ifndef LINUX_REBOOT_H
#define LINUX_REBOOT_H

#include <tee_internal_api.h>

void set_nsec_entry_reboot(unsigned long nsec_entry, unsigned long dt_addr,
			   unsigned long args);

TEE_Result update_image(void **img, size_t *size);
TEE_Result prepare_normal_world(void *img, size_t size);
TEE_Result prepare_normal_world_with_copy(void *img, size_t size,
					  void *(copy)(void *dest,
						       const void *src,
						       size_t len));
void restart_normal_world(void);

#endif /* LINUX_REBOOT_H */
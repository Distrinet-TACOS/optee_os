// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2014, STMicroelectronics International N.V.
 * Copyright (c) 2015-2020 Linaro Limited
 */

#include <initcall.h>
#include <kernel/linker.h>
#include <kernel/user_access.h>
#include <kernel/user_mode_ctx.h>
#include <mm/vm.h>
#include <string.h>
#include <tee_api_types.h>
#include <types_ext.h>

static struct user_mode_ctx *get_current_uctx(void)
{
	struct ts_session *s = ts_get_current_session();

	if (!is_user_mode_ctx(s->ctx)) {
		/*
		 * We may be called within a PTA session, which doesn't
		 * have a user_mode_ctx. Here, try to retrieve the
		 * user_mode_ctx associated with the calling session.
		 */
		s = TAILQ_NEXT(s, link_tsd);
		if (!s || !is_user_mode_ctx(s->ctx))
			return NULL;
	}

	return to_user_mode_ctx(s->ctx);
}

static TEE_Result check_access(uint32_t flags, const void *uaddr, size_t len)
{
	struct user_mode_ctx *uctx = get_current_uctx();

	if (!uctx)
		return TEE_ERROR_GENERIC;

	return vm_check_access_rights(uctx, flags, (vaddr_t)uaddr, len);
}

TEE_Result copy_from_user(void *kaddr, const void *uaddr, size_t len)
{
	uint32_t flags = TEE_MEMORY_ACCESS_READ | TEE_MEMORY_ACCESS_ANY_OWNER;
	TEE_Result res = check_access(flags, uaddr, len);

	if (!res)
		memcpy(kaddr, uaddr, len);

	return res;
}

TEE_Result copy_to_user(void *uaddr, const void *kaddr, size_t len)
{
	uint32_t flags = TEE_MEMORY_ACCESS_WRITE | TEE_MEMORY_ACCESS_ANY_OWNER;
	TEE_Result res = check_access(flags, uaddr, len);

	if (!res)
		memcpy(uaddr, kaddr, len);

	return res;
}

TEE_Result copy_from_user_private(void *kaddr, const void *uaddr, size_t len)
{
	uint32_t flags = TEE_MEMORY_ACCESS_READ;
	TEE_Result res = check_access(flags, uaddr, len);

	if (!res)
		memcpy(kaddr, uaddr, len);

	return res;
}

TEE_Result copy_to_user_private(void *uaddr, const void *kaddr, size_t len)
{
	uint32_t flags = TEE_MEMORY_ACCESS_WRITE;
	TEE_Result res = check_access(flags, uaddr, len);

	if (!res)
		memcpy(uaddr, kaddr, len);

	return res;
}

TEE_Result copy_kaddr_to_uref(uint32_t *uref, void *kaddr)
{
	uint32_t ref = kaddr_to_uref(kaddr);

	return copy_to_user_private(uref, &ref, sizeof(ref));
}

uint32_t kaddr_to_uref(void *kaddr)
{
	assert(((vaddr_t)kaddr - VCORE_START_VA) < UINT32_MAX);
	return (vaddr_t)kaddr - VCORE_START_VA;
}

vaddr_t uref_to_vaddr(uint32_t uref)
{
	return VCORE_START_VA + uref;
}

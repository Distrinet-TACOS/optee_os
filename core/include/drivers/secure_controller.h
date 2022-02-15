#ifndef SECURE_CONTROLLER
#define SECURE_CONTROLLER

#include <tee_api_types.h>

// TEE_Result sec_ssp_register(TEE_UUID uuid);
TEE_Result sec_ssp_notify(TEE_UUID uuid);

#endif // SECURE_CONTROLLER
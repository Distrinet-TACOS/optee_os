// File: optee_os/core/pta/testpta.c

#include <kernel/pseudo_ta.h>

#define PTA_NAME "test.pta"
#define PTA_TEST_PRINT_UUID { 0xd4be3f91, 0xc4e1, 0x436c, \
    { 0xb2, 0x92, 0xbf, 0xf5, 0x3e, 0x43, 0x04, 0xd5 } }

#define PTA_TEST_PRINT 0

static TEE_Result test_print(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]) {
    static int counter = 0;
    IMSG("This is a test of a pseudo trusted application.");
    IMSG("Counter = %d", counter);
    return TEE_SUCCESS;
}

/*
 * Trusted Application Etnry Points
 */

static TEE_Result invoke_command(void *session __unused, uint32_t cmd,
                                 uint32_t ptypes,
                                 TEE_Param params[TEE_NUM_PARAMS]) {
  switch (cmd) {
    case PTA_TEST_PRINT:
        return test_print(ptypes, params);
        break;
    default:
        break;
  }

  return TEE_ERROR_NOT_IMPLEMENTED;
}

pseudo_ta_register(.uuid = PTA_TEST_PRINT_UUID, .name = PTA_NAME,
    .flags = PTA_DEFAULT_FLAGS, .invoke_command_entry_point = invoke_command);
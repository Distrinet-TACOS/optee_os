#include <testpta.h>
#include <kernel/pseudo_ta.h>
#include <platform_config.h>
#include <kernel/interrupt.h>
#include <console.h>
#include <kernel/misc.h>
#include <initcall.h>

#define PTA_NAME "test.pta"
#define PTA_TEST_PRINT_UUID { 0xd4be3f91, 0xc4e1, 0x436c, \
    { 0xb2, 0x92, 0xbf, 0xf5, 0x3e, 0x43, 0x04, 0xd5 } }

#define PTA_REGISTER_ITR 0
#define PTA_DISABLE_ITR 1

void register_serial_chip(struct serial_chip *chip) {
    serial_chip = chip;
}

static enum itr_return console_itr_cb(struct itr_handler *h __unused)
{
    while (serial_chip->ops->have_rx_data(serial_chip)) {
        int ch = serial_chip->ops->getchar(serial_chip);
		DMSG("new cpu %zu: got 0x%x", get_core_pos(), ch);
    }

    return ITRR_HANDLED;
}

static enum itr_return test(void) {
    return ITRR_NONE;
}

static struct itr_handler console_itr = {
    .it = IT_CONSOLE_UART,
    .flags = ITRF_TRIGGER_LEVEL,
    .handler = console_itr_cb,
};
DECLARE_KEEP_PAGER(console_itr);

static TEE_Result register_itr(void) {
    IMSG("Registering console interrupt.\n");
    itr_add(&console_itr);
    itr_set_affinity(console_itr.it, 1);
    itr_enable(console_itr.it);

    return TEE_SUCCESS;
}

static TEE_Result disable_itr(void) {
    IMSG("Disabling console interrupt.\n");
    itr_disable(IT_CONSOLE_UART);

    return TEE_SUCCESS;
}

static TEE_Result open_session(uint32_t nParamTypes __unused,
			TEE_Param pParams[TEE_NUM_PARAMS] __unused,
			void **ppSessionContext __unused) {
    
    IMSG("Session opened.");
    return TEE_SUCCESS;
}

static TEE_Result invoke_command(void *session __unused, uint32_t cmd,
                                 uint32_t ptypes __unused,
                                 TEE_Param params[TEE_NUM_PARAMS] __unused) {
  switch (cmd) {
    case PTA_REGISTER_ITR:
        return register_itr();
        break;
    case PTA_DISABLE_ITR:
        return disable_itr();
    default:
        break;
  }

  return TEE_ERROR_NOT_IMPLEMENTED;
}

pseudo_ta_register(.uuid = PTA_TEST_PRINT_UUID, .name = PTA_NAME,
    .flags = PTA_DEFAULT_FLAGS, .invoke_command_entry_point = invoke_command,
    .open_session_entry_point = open_session);
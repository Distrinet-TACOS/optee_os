#include <drivers/console_split.h>
#include <kernel/thread.h>
#include <kernel/pseudo_ta.h>
#include <kernel/interrupt.h>
#include <kernel/notif.h>
#include <string.h>

static struct serial_chip *serial_chip_con_split;
static enum itr_return console_split_itr_cb(struct itr_handler *handler __unused);
static uint32_t notif_value = -1;

static struct itr_handler console_itr = {
	.it = IT_CONSOLE_UART,
	.flags = ITRF_TRIGGER_LEVEL,
	.handler = console_split_itr_cb,
};
DECLARE_KEEP_PAGER(console_itr);
static bool itr_registered = false;

void register_serial_chip_con_split(struct serial_chip *chip)
{
	serial_chip_con_split = chip;
}

static enum itr_return console_split_itr_cb(struct itr_handler *handler __unused)
{
	// Send notification that a serial interrupt was registered.
	// Disable interrupt in the meantime.
	itr_disable(console_itr.it);
	IMSG("Raising interrupt.\n");
	notif_send_async(notif_value);

	return ITRR_HANDLED;
}

static int read_console(void)
{
	int ch = -1;

	while (serial_chip_con_split->ops->have_rx_data(
		serial_chip_con_split)) {
		ch = serial_chip_con_split->ops->getchar(serial_chip_con_split);
	}

	return ch;
}

static TEE_Result read_char(uint32_t ptypes, TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t expected_ptypes =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT,
				TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE);

	if (ptypes != expected_ptypes)
		return TEE_ERROR_BAD_PARAMETERS;

	params[0].value.a = read_console();

	// Re-enable interrupt
	itr_enable(console_itr.it);

	return TEE_SUCCESS;
}

static TEE_Result write_chars(uint32_t ptypes, TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t expected_ptypes =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
				TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE);

	if (ptypes != expected_ptypes)
		return TEE_ERROR_BAD_PARAMETERS;

	char *buf = params[0].memref.buffer;
	uint32_t size = params[0].memref.size;
	char local[size];

	memcpy(local, buf, size);

	IMSG("Received %s\n", local);

	return TEE_SUCCESS;
}

static TEE_Result register_itr(uint32_t ptypes,
			       TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t expected_ptypes =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT,
				TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE);

	if (ptypes != expected_ptypes)
		return TEE_ERROR_BAD_PARAMETERS;

	IMSG("Registering console interrupt.\n");

	// Try to get async notification value. Return error if failed.
	TEE_Result res = notif_alloc_async_value(&notif_value);
	if (res != TEE_SUCCESS)
		return res;

	IMSG("Notification value: %d.\n", notif_value);
	params[0].value.a = notif_value;

	// Only add the interrupt the first time this function is called, as no
	// function in interrupt.h is  provided to only remove and not free the
	// interrupt.
	if (!itr_registered) {
		itr_add(&console_itr);
		itr_registered = true;
	}
	itr_set_affinity(console_itr.it, 1);
	itr_enable(console_itr.it);

	IMSG("Console interrupt registered and enabled.\n");

	return TEE_SUCCESS;
}

static TEE_Result unregister_itr(uint32_t ptypes,
				 TEE_Param params[TEE_NUM_PARAMS] __unused)
{
	uint32_t expected_ptypes =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE);

	if (ptypes != expected_ptypes)
		return TEE_ERROR_BAD_PARAMETERS;

	IMSG("Unregistering console interrupt.\n");

	// Free notification value.
	notif_free_async_value(notif_value);

	IMSG("Notification value %d freed.\n", notif_value);

	itr_disable(console_itr.it);

	IMSG("Console interrupt disabled and unregistered.\n");

	return TEE_SUCCESS;
}

static TEE_Result open_session(uint32_t nParamTypes __unused,
			       TEE_Param pParams[TEE_NUM_PARAMS] __unused,
			       void **ppSessionContext __unused)
{
	IMSG("Session opened.\n");
	return TEE_SUCCESS;
}

static TEE_Result invoke_command(void *session __unused, uint32_t cmd,
				 uint32_t ptypes,
				 TEE_Param params[TEE_NUM_PARAMS])
{
	switch (cmd) {
	case REGISTER_ITR:
		IMSG("Register interrupt called.\n");
		return register_itr(ptypes, params);
	case UNREGISTER_ITR:
		IMSG("Unregister interrupt called.\n");
		return unregister_itr(ptypes, params);
	case READ_CHAR:
		return read_char(ptypes, params);
	case WRITE_CHARS:
		return write_chars(ptypes, params);
	default:
		return TEE_ERROR_NOT_IMPLEMENTED;
	}
}

pseudo_ta_register(.uuid = PTA_CONSOLE_SPLIT_UUID, .name = PTA_NAME,
		   .flags = PTA_DEFAULT_FLAGS,
		   .invoke_command_entry_point = invoke_command,
		   .open_session_entry_point = open_session);

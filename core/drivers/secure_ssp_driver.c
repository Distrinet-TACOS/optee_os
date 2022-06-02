#include <kernel/thread.h>
#include <kernel/pseudo_ta.h>
#include <kernel/ts_manager.h>
#include <kernel/tee_ta_manager.h>
#include <kernel/interrupt.h>
#include <kernel/notif.h>
#include <kernel/trace_ta.h>
#include <string.h>
#include <sys/queue.h>

#include <drivers/secure_ssp_driver.h>
#include <drivers/secure_ssp_driver_public.h>
#include <drivers/secure_controller.h>

#define SEC_SSP_DRIVER_UUID                                                    \
	{                                                                      \
		UUID1, UUID2, UUID3,                                           \
		{                                                              \
			UUID4, UUID5, UUID6, UUID7, UUID8, UUID9, UUID10,      \
				UUID11                                         \
		}                                                              \
	}
static TEE_UUID sec_ssp_driver_uuid = SEC_SSP_DRIVER_UUID;

static enum itr_return
console_split_itr_cb(struct itr_handler *handler __unused);

static struct secure_ssp_driver {
	struct serial_chip *serial_chip_con_split;
	bool itr_registered;
	unsigned int itr_count;
} sec_driver = { .itr_registered = false, .itr_count = 0 };

struct buf_entry {
	char c;
	STAILQ_ENTRY(buf_entry) queue;
};

struct device {
	bool itr_enabled;
	unsigned int buf_len;
	STAILQ_HEAD(buf_head, buf_entry) buffer;
};

union {
	struct {
		struct device *norm_dev, *sec_dev;
	};
	struct device *dev_array[2];
} devices = { { NULL, NULL } };

#define WRITE_PREFIX 	"Normal World"
#define OUT_BUFF_SIZE	500
static char out_buff[OUT_BUFF_SIZE] = {0};

static struct itr_handler console_itr = {
	// .it = CFG_UART_BASE,
	.it = 59,
	.flags = ITRF_TRIGGER_LEVEL,
	.handler = console_split_itr_cb,
};
DECLARE_KEEP_PAGER(console_itr);

void register_serial_chip_con_split(struct serial_chip *chip)
{
	sec_driver.serial_chip_con_split = chip;
}

static enum itr_return
console_split_itr_cb(struct itr_handler *handler __unused)
{
	struct serial_chip *chip = sec_driver.serial_chip_con_split;
	int i = 0;
	struct device *device;
	struct buf_entry *entry;

	while (chip->ops->have_rx_data(chip)) {
		char c = chip->ops->getchar(chip);

		for (i = 0; i < 2; i++) {
			device = devices.dev_array[i];
			if (device && device->itr_enabled &&
			    device->buf_len <= BUFFER_SIZE) {
				entry = malloc(sizeof(struct buf_entry));
				entry->c = c;
				STAILQ_INSERT_TAIL(&device->buffer, entry,
						   queue);
				device->buf_len++;
			}
		}
	}

	// Send notification that a serial interrupt was received.
	TEE_Result res = sec_ssp_notify(sec_ssp_driver_uuid);
	if (res) {
		EMSG("Error sending notification.\n");
	}

	return ITRR_HANDLED;
}

static TEE_Result write_chars(bool secure __unused, uint32_t ptypes,
			      TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t expected_ptypes =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
				TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE);

	if (ptypes != expected_ptypes)
		return TEE_ERROR_BAD_PARAMETERS;

	char *buf = params[0].memref.buffer;
	unsigned int size = params[0].memref.size;

	memset(out_buff, 0, OUT_BUFF_SIZE);
	memcpy(out_buff, buf, size);

	IMSG("%s: %s", WRITE_PREFIX, out_buff);

	return TEE_SUCCESS;
}

static TEE_Result register_itr(bool secure)
{
	struct device *device = secure ? devices.sec_dev : devices.norm_dev;

	// Only add the interrupt the first time this function
	// is called, as no function in interrupt.h is provided
	// to only remove and not free the interrupt.
	if (!sec_driver.itr_registered) {
		itr_add(&console_itr);
		sec_driver.itr_registered = true;
		itr_set_affinity(console_itr.it, 1);
	}

	if (sec_driver.itr_count == 0) {
		itr_enable(console_itr.it);
	}

	if (!device->itr_enabled) {
		device->itr_enabled = true;
		sec_driver.itr_count++;
	}
	return TEE_SUCCESS;
}

static TEE_Result unregister_itr(bool secure)
{
	struct device *device = secure ? devices.sec_dev : devices.norm_dev;

	if (device->itr_enabled) {
		device->itr_enabled = false;
		sec_driver.itr_count--;
	}

	if (sec_driver.itr_registered && sec_driver.itr_count == 0) {
		itr_disable(console_itr.it);
	}

	return TEE_SUCCESS;
}

static TEE_Result update_buffer(bool secure, uint32_t ptypes,
				TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t expected_ptypes =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT,
				TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE);

	if (ptypes != expected_ptypes)
		return TEE_ERROR_BAD_PARAMETERS;

	struct device *device = secure ? devices.sec_dev : devices.norm_dev;

	if (params[0].memref.size < device->buf_len * sizeof(char)) {
		params[0].memref.size = device->buf_len;
		return TEE_ERROR_SHORT_BUFFER;
	}

	params[0].memref.size = device->buf_len * sizeof(char);
	char *buffer = params[0].memref.buffer;
	int i = 0;

	while (!STAILQ_EMPTY(&device->buffer)) {
		// Remove first element from buffer and add to update buffer.
		struct buf_entry *e = STAILQ_FIRST(&device->buffer);
		buffer[i++] = e->c;
		STAILQ_REMOVE_HEAD(&device->buffer, queue);
		device->buf_len--;
		free(e);
	}

	return TEE_SUCCESS;
}

static TEE_Result open_session(uint32_t ptypes __unused,
			       TEE_Param params[TEE_NUM_PARAMS] __unused,
			       void **ppSessionContext)
{
	// Get session id and save if the calling application is a secure or
	// non-secure application.
	bool *secure;

	struct ts_session *s = ts_get_calling_session();
	if (s && is_ta_ctx(s->ctx) && !devices.sec_dev) {
		secure = malloc(sizeof(bool));
		*secure = true;

		devices.sec_dev = malloc(sizeof(struct device));
		memset(devices.sec_dev, 0, sizeof(struct device));
		devices.sec_dev->itr_enabled = false;

		STAILQ_INIT(&devices.sec_dev->buffer);
	} else if (!devices.norm_dev) {
		secure = malloc(sizeof(bool));
		*secure = false;

		devices.norm_dev = malloc(sizeof(struct device));
		memset(devices.norm_dev, 0, sizeof(struct device));
		devices.norm_dev->itr_enabled = false;

		STAILQ_INIT(&devices.norm_dev->buffer);
	} else {
		return TEE_ERROR_ACCESS_CONFLICT;
	}

	*ppSessionContext = secure;
	return TEE_SUCCESS;
}

static TEE_Result create_entry_point(void)
{
	// return sec_ssp_register(sec_ssp_driver_uuid);
	return TEE_SUCCESS;
}

static TEE_Result invoke_command(void *pSessionContext, uint32_t cmd,
				 uint32_t ptypes,
				 TEE_Param params[TEE_NUM_PARAMS])
{
	bool secure = *(bool *)pSessionContext;
	switch (cmd) {
	case REGISTER_ITR:
		return register_itr(secure);
	case UNREGISTER_ITR:
		return unregister_itr(secure);
	case WRITE_CHARS:
		return write_chars(secure, ptypes, params);
	case UPDATE_BUFFER:
		return update_buffer(secure, ptypes, params);
	default:
		return TEE_ERROR_NOT_IMPLEMENTED;
	}
}

pseudo_ta_register(.uuid = SEC_SSP_DRIVER_UUID, .name = PTA_NAME,
		   .flags = PTA_DEFAULT_FLAGS,
		   .invoke_command_entry_point = invoke_command,
		   .open_session_entry_point = open_session,
		   .create_entry_point = create_entry_point);

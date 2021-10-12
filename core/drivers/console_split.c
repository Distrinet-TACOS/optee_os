#include <kernel/thread.h>
#include <kernel/pseudo_ta.h>
#include <kernel/ts_manager.h>
#include <kernel/tee_ta_manager.h>
#include <kernel/interrupt.h>
#include <kernel/notif.h>
#include <string.h>
#include <sys/queue.h>

#include <drivers/console_split.h>
#include <drivers/console_split_public.h>

#define PTA_CONSOLE_SPLIT_UUID                                                 \
	{                                                                      \
		UUID1, UUID2, UUID3,                                           \
		{                                                              \
			UUID4, UUID5, UUID6, UUID7, UUID8, UUID9, UUID10,      \
				UUID11                                         \
		}                                                              \
	}

static enum itr_return
console_split_itr_cb(struct itr_handler *handler __unused);

static struct secure_controller {
	struct serial_chip *serial_chip_con_split;
	uint32_t notif_value;
	bool itr_registered;
	unsigned int itr_count;
} sec_data = { .notif_value = -1, .itr_registered = false, .itr_count = 0 };

struct buf_entry {
	char c;
	STAILQ_ENTRY(buf_entry) queue;
};

struct split_device {
	uint32_t sess_id;
	bool itr_enabled;
	STAILQ_HEAD(buf_head, buf_entry) buffer;
	LIST_ENTRY(split_device) list;
};

LIST_HEAD(split_device_head, split_device)
devices = LIST_HEAD_INITIALIZER(devices);

static struct itr_handler console_itr = {
	.it = IT_CONSOLE_UART,
	.flags = ITRF_TRIGGER_LEVEL,
	.handler = console_split_itr_cb,
};
DECLARE_KEEP_PAGER(console_itr);

void register_serial_chip_con_split(struct serial_chip *chip)
{
	sec_data.serial_chip_con_split = chip;
}

static enum itr_return
console_split_itr_cb(struct itr_handler *handler __unused)
{
	struct serial_chip *chip = sec_data.serial_chip_con_split;
	struct split_device *device;
	struct buf_entry *entry;

	while (chip->ops->have_rx_data(chip)) {
		char c = chip->ops->getchar(chip);

		LIST_FOREACH(device, &devices, list)
		{
			if (device->itr_enabled) {
				entry = malloc(sizeof(struct buf_entry));
				entry->c = c;
				STAILQ_INSERT_TAIL(&device->buffer, entry,
						   queue);
			}
		}
	}

	// Send notification that a serial interrupt was received.
	notif_send_async(sec_data.notif_value);

	return ITRR_HANDLED;
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

static TEE_Result register_itr(uint32_t sess_id)
{
	struct split_device *device;

	LIST_FOREACH(device, &devices, list)
	{
		if (device->sess_id == sess_id) {
			// Only add the interrupt the first time this function
			// is called, as no function in interrupt.h is provided
			// to only remove and not free the interrupt.
			if (!sec_data.itr_registered) {
				itr_add(&console_itr);
				sec_data.itr_registered = true;
				itr_set_affinity(console_itr.it, 1);
			}

			if (sec_data.itr_count == 0) {
				itr_enable(console_itr.it);
			}

			if (!device->itr_enabled) {
				device->itr_enabled = true;
				sec_data.itr_count++;
			}
			return TEE_SUCCESS;
		}
	}

	return TEE_ERROR_ITEM_NOT_FOUND;
}

static TEE_Result unregister_itr(uint32_t sess_id)
{
	struct split_device *device;

	LIST_FOREACH(device, &devices, list)
	{
		if (device->sess_id == sess_id) {
			if (device->itr_enabled) {
				device->itr_enabled = false;
				sec_data.itr_count--;
			}

			if (sec_data.itr_registered &&
			    sec_data.itr_count == 0) {
				itr_disable(console_itr.it);
			}

			return TEE_SUCCESS;
		}
	}

	return TEE_ERROR_ITEM_NOT_FOUND;
}

static TEE_Result enable_notif(uint32_t ptypes,
			       TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t expected_ptypes =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT,
				TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE);

	if (ptypes != expected_ptypes)
		return TEE_ERROR_BAD_PARAMETERS;

	if (sec_data.notif_value > 0) {
		// Try to get async notification value. Return error if failed.
		TEE_Result res = notif_alloc_async_value(&sec_data.notif_value);
		if (res != TEE_SUCCESS)
			return TEE_ERROR_OUT_OF_MEMORY;
	}

	IMSG("Notification value: %d.\n", sec_data.notif_value);
	params[0].value.a = sec_data.notif_value;

	return TEE_SUCCESS;
}

static TEE_Result disable_notif(void)
{
	if (sec_data.notif_value > 0) {
		notif_free_async_value(sec_data.notif_value);
	}

	return TEE_SUCCESS;
}

static TEE_Result update_buffer(uint32_t ptypes,
				TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t expected_ptypes =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT,
				TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE);

	if (ptypes != expected_ptypes)
		return TEE_ERROR_BAD_PARAMETERS;

	struct update_buffer *current = params[0].memref.buffer;

	struct split_device *device;

	LIST_FOREACH(device, &devices, list)
	{
		while (!STAILQ_EMPTY(&device->buffer)) {
			if (!current || current->count == 0) {
				// Init update buffer if it doesn't exist.
				current->sess_id = device->sess_id;
			}

			// Remove first element from buffer and add to update buffer.
			struct buf_entry *e = STAILQ_FIRST(&device->buffer);
			current->buf[current->count++] = e->c;
			STAILQ_REMOVE_HEAD(&device->buffer, queue);
			free(e);
		}

		current = &current[1];
	}

	return TEE_SUCCESS;
}

static TEE_Result open_session(uint32_t ptypes,
			       TEE_Param params[TEE_NUM_PARAMS],
			       void **ppSessionContext)
{
	uint32_t expected_ptypes =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT, TEE_PARAM_TYPE_NONE,
				TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE);

	if (ptypes != expected_ptypes)
		return TEE_ERROR_BAD_PARAMETERS;

	// Get session id and save it.
	struct ts_session *s = ts_get_current_session();
	struct tee_ta_session *sess = to_ta_session(s);
	*ppSessionContext = &sess->id;

	bool create_buf = params[0].value.a;

	// Add data structure for device.
	if (create_buf) {
		struct split_device *dev = malloc(sizeof(struct split_device));
		dev->sess_id = sess->id;
		dev->itr_enabled = false;
		STAILQ_INIT(&dev->buffer);
		LIST_INSERT_HEAD(&devices, dev, list);
	}

	IMSG("Session %u opened.\n", sess->id);
	return TEE_SUCCESS;
}

static void close_session(void *pSessionContext)
{
	uint32_t sess_id = *(uint32_t *)pSessionContext;
	struct split_device *device;

	LIST_FOREACH(device, &devices, list)
	{
		if (device->sess_id == sess_id) {
			LIST_REMOVE(device, list);
			free(device);
			return;
		}
	}
}

static TEE_Result invoke_command(void *pSessionContext, uint32_t cmd,
				 uint32_t ptypes,
				 TEE_Param params[TEE_NUM_PARAMS])
{
	uint32_t sess_id = *(uint32_t *)pSessionContext;
	switch (cmd) {
	case REGISTER_ITR:
		IMSG("REGISTER_ITR called.\n");
		return register_itr(sess_id);
	case UNREGISTER_ITR:
		IMSG("UNREGISTER_ITR called.\n");
		return unregister_itr(sess_id);
	case WRITE_CHARS:
		IMSG("WRITE_CHARS called.\n");
		return write_chars(ptypes, params);
	case ENABLE_NOTIF:
		IMSG("ENABLE_NOTIF called.\n");
		return enable_notif(ptypes, params);
	case DISABLE_NOTIF:
		IMSG("DISABLE_NOTIF called.\n");
		return disable_notif();
	case UPDATE_BUFFER:
		IMSG("UPDATE_BUFFER called.\n");
		return update_buffer(ptypes, params);
	default:
		return TEE_ERROR_NOT_IMPLEMENTED;
	}
}

pseudo_ta_register(.uuid = PTA_CONSOLE_SPLIT_UUID, .name = PTA_NAME,
		   .flags = PTA_DEFAULT_FLAGS,
		   .invoke_command_entry_point = invoke_command,
		   .open_session_entry_point = open_session,
		   .close_session_entry_point = close_session);

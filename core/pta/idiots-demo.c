#include <tee_api_types.h>
#include <kernel/pseudo_ta.h>
#include <tee/uuid.h>
#include <io.h>
#include <mm/core_memprot.h>

#define PTA_NAME "IDIoTS demo"
#define IDIOTS_DEMO_UUID                                                       \
        {                                                                      \
                0x8aaaf200, 0x2450, 0x11e4,                                    \
                {                                                              \
                        0xab, 0xe2, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b         \
                }                                                              \
        }

#define TA_IDIOTS_NOTIFY 0
#define TA_IDIOTS_CHANGE 1
#define TA_IDIOTS_PRINT 2

#define UART1 0x02020000

/* Register definitions */
#define URXD 0x0 /* Receiver Register */
#define UTXD 0x40 /* Transmitter Register */
#define UCR1 0x80 /* Control Register 1 */
#define UCR2 0x84 /* Control Register 2 */
#define UCR3 0x88 /* Control Register 3 */
#define UCR4 0x8c /* Control Register 4 */
#define UFCR 0x90 /* FIFO Control Register */
#define USR1 0x94 /* Status Register 1 */
#define USR2 0x98 /* Status Register 2 */
#define UESC 0x9c /* Escape Character Register */
#define UTIM 0xa0 /* Escape Timer Register */
#define UBIR 0xa4 /* BRM Incremental Register */
#define UBMR 0xa8 /* BRM Modulator Register */
#define UBRC 0xac /* Baud Rate Count Register */
#define UTS 0xb4 /* UART Test Register (mx31) */
#define UMCR 0xb8 /* UART RS-485 Mode Control Register */
#define USIZE UMCR + sizeof(uint32_t) /* UMCR + sizeof(uint32_t) */

static vaddr_t base;
static bool mapped = false;

static TEE_Result notify(uint32_t ptypes, TEE_Param params[TEE_NUM_PARAMS])
{
        vaddr_t addr;

        if (ptypes != TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT,
                                      TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE,
                                      TEE_PARAM_TYPE_NONE))
                return TEE_ERROR_BAD_PARAMETERS;

        if (!mapped) {
                addr = (vaddr_t)core_mmu_add_mapping(MEM_AREA_RAM_NSEC,
                                            (paddr_t)params[0].value.a, 1);
        } else {
                addr = (vaddr_t)phys_to_virt((paddr_t)params[0].value.a,
                                             MEM_AREA_RAM_NSEC, 1);
        }

        IMSG("a = %d", (int)io_read32(addr));

        return TEE_SUCCESS;
}

static TEE_Result change(uint32_t ptypes, TEE_Param params[TEE_NUM_PARAMS])
{
        vaddr_t addr;

        if (ptypes != TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT,
                                      TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE,
                                      TEE_PARAM_TYPE_NONE))
                return TEE_ERROR_BAD_PARAMETERS;

        if (!mapped) {
                addr = (vaddr_t)core_mmu_add_mapping(MEM_AREA_RAM_NSEC,
                                            (paddr_t)params[0].value.a, 1);
        } else {
                addr = (vaddr_t)phys_to_virt((paddr_t)params[0].value.a,
                                             MEM_AREA_RAM_NSEC, 1);
        }

        IMSG("Setting a to 100\n");
        io_write32(addr, 100);

        return TEE_SUCCESS;
}

static TEE_Result print(void)
{
        IMSG("Normal world UART config registers:\n");
        IMSG("  URXD: 0x%08x\n", io_read32(base + URXD));
        IMSG("  UTXD: 0x%08x\n", io_read32(base + UTXD));
        IMSG("  UCR1: 0x%08x\n", io_read32(base + UCR1));
        IMSG("  UCR2: 0x%08x\n", io_read32(base + UCR2));
        IMSG("  UCR3: 0x%08x\n", io_read32(base + UCR3));
        IMSG("  UCR4: 0x%08x\n", io_read32(base + UCR4));
        IMSG("  UFCR: 0x%08x\n", io_read32(base + UFCR));
        IMSG("  USR1: 0x%08x\n", io_read32(base + USR1));
        IMSG("  USR2: 0x%08x\n", io_read32(base + USR2));

        return TEE_SUCCESS;
}

static TEE_Result invoke_command(void *pSessionContext __unused, uint32_t cmd,
                                 uint32_t ptypes,
                                 TEE_Param params[TEE_NUM_PARAMS])
{
        switch (cmd) {
        case TA_IDIOTS_NOTIFY:
                return notify(ptypes, params);
        case TA_IDIOTS_CHANGE:
                return change(ptypes, params);
        case TA_IDIOTS_PRINT:
                return print();
        default:
                return TEE_ERROR_NOT_IMPLEMENTED;
        }
}

static TEE_Result create_entry_point(void)
{
        struct io_pa_va k = { .pa = UART1 };
        base = io_pa_or_va(&k, USIZE);

        return TEE_SUCCESS;
}

pseudo_ta_register(.uuid = IDIOTS_DEMO_UUID, .name = PTA_NAME,
                   .flags = PTA_MANDATORY_FLAGS,
                   .invoke_command_entry_point = invoke_command,
                   .create_entry_point = create_entry_point);

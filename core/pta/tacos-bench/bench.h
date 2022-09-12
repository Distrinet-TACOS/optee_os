#ifndef TACOS_BENCH_H
#define TACOS_BENCH_H

#include <tee_api_types.h>

#define EPITCR 0x00
#define EPITSR 0x04
#define EPITLR 0x08
#define EPITCMPR 0x0c
#define EPITCNR 0x10

#define EPITCR_EN BIT(0)
#define EPITCR_ENMOD BIT(1)
#define EPITCR_OCIEN BIT(2)
#define EPITCR_RLD BIT(3)
#define EPITCR_PRESC(x) (((x)&0xfff) << 4)
#define EPITCR_SWR BIT(16)
#define EPITCR_IOVW BIT(17)
#define EPITCR_DBGEN BIT(18)
#define EPITCR_WAITEN BIT(19)
#define EPITCR_RES BIT(20)
#define EPITCR_STOPEN BIT(21)
#define EPITCR_OM_DISCON (0 << 22)
#define EPITCR_OM_TOGGLE (1 << 22)
#define EPITCR_OM_CLEAR (2 << 22)
#define EPITCR_OM_SET (3 << 22)
#define EPITCR_CLKSRC_OFF (0 << 24)
#define EPITCR_CLKSRC_PERIPHERAL (1 << 24)
#define EPITCR_CLKSRC_REF_HIGH (2 << 24)
#define EPITCR_CLKSRC_REF_LOW (3 << 24)

#define EPITSR_OCIF BIT(0)

#define MAX_STAMPS 100
#define stamp(x) asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(x))

enum bench_cmd { RTT, ASYNC, RTT_MEM, PRINT_MEM, EPIT_SW, EPIT_NW };

struct timestamps {
	uint32_t counter;
	uint32_t stamps[MAX_STAMPS];
};

extern struct bench {
	struct timestamps *mem;
	uint32_t size;
	struct serial_chip *serial_chip_con_split;
	uint32_t notif_value;
	bool itr_enabled;
	struct timestamps stamps;
	vaddr_t gt_addr;
} bench;

void _print_stamps(enum bench_cmd cmd);

TEE_Result epit_sw(void);
TEE_Result epit_nw(uint32_t ptypes, TEE_Param params[TEE_NUM_PARAMS]);
void add_epit_itr(void);

#endif /* TACOS_BENCH_H */
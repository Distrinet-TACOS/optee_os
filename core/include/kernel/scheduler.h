#ifndef SCHEDULER_H
#define SCHEDULER_H


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
#define EPITCR_CLKSRC_PERIPHERAL (1 << c 24)
#define EPITCR_CLKSRC_REF_HIGH (2 << 24)
#define EPITCR_CLKSRC_REF_LOW (3 << 24)

#define EPITSR_OCIF BIT(0)

#define EPIT_BASE_PA 0x20d0000

/* Clear EPIT Interrupt */
void vClearEpitInterrupt(void);

#endif /* SCHEDULER_H */
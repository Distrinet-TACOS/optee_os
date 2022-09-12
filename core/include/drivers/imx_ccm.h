#ifndef IMX_CCM_H
#define IMX_CCM_H

#include <stdint.h>

void read_clk_settings(void);
void ungate_clk(uint32_t offset, uint32_t shift);

#endif /* IMX_CCM_H */

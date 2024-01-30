#ifndef PTA_UPDATER_H
#define PTA_UPDATER_H

#include <stdint.h>

#define PTA_NAME "updater.pta"

// b84653a5-753a-4fe1-b187-0ccc6a7cdd83
#define PTA_UPDATER_UUID                                               \
	{                                                              \
		0xb84653a5, 0x753a, 0x4fe1,                            \
		{                                                      \
			0xb1, 0x87, 0x0c, 0xcc, 0x6a, 0x7c, 0xdd, 0x83 \
		}                                                      \
	}

/* The function IDs implemented in this TA */
#define PTA_UPDATER_CMD_UPDATE	    0
#define PTA_UPDATER_CMD_ICREASE_MEM 1

#endif /*PTA_UPDATER_H*/

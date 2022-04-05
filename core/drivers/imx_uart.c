// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 * All rights reserved.
 * Copyright 2018-2019 NXP.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <drivers/imx_uart.h>
#include <io.h>
#include <keep.h>
#include <kernel/dt.h>
#include <util.h>

/* Register definitions */
#define URXD 	0x0	/* Receiver Register */
#define UTXD 	0x40	/* Transmitter Register */
#define UCR1 	0x80	/* Control Register 1 */
#define UCR2 	0x84	/* Control Register 2 */
#define UCR3 	0x88	/* Control Register 3 */
#define UCR4 	0x8c	/* Control Register 4 */
#define UFCR 	0x90	/* FIFO Control Register */
#define USR1 	0x94	/* Status Register 1 */
#define USR2 	0x98	/* Status Register 2 */
#define UESC 	0x9c	/* Escape Character Register */
#define UTIM 	0xa0	/* Escape Timer Register */
#define UBIR 	0xa4	/* BRM Incremental Register */
#define UBMR 	0xa8	/* BRM Modulator Register */
#define UBRC 	0xac	/* Baud Rate Count Register */
#define UTS 	0xb4	/* UART Test Register (mx31) */
#define UMCR	0xb8	/* UART RS-485 Mode Control Register */
#define USIZE 	UMCR + sizeof(uint32_t) /* UMCR + sizeof(uint32_t) */

/* UART Control Register Bit Fields. */
#define URXD_CHARRDY	(1 << 15)
#define URXD_ERR	(1 << 14)
#define URXD_OVRRUN	(1 << 13)
#define URXD_FRMERR	(1 << 12)
#define URXD_BRK	(1 << 11)
#define URXD_PRERR	(1 << 10)
#define URXD_RX_DATA	(0xFF)
#define UCR1_ADEN	(1 << 15)	/* Auto dectect interrupt */
#define UCR1_ADBR	(1 << 14)	/* Auto detect baud rate */
#define UCR1_TRDYEN	(1 << 13)	/* Transmitter ready interrupt enable */
#define UCR1_IDEN	(1 << 12)	/* Idle condition interrupt */
#define UCR1_RRDYEN	(1 << 9)	/* Recv ready interrupt enable */
#define UCR1_RDMAEN	(1 << 8)	/* Recv ready DMA enable */
#define UCR1_IREN	(1 << 7)	/* Infrared interface enable */
#define UCR1_TXMPTYEN	(1 << 6)	/* Transimitter empty interrupt enable */
#define UCR1_RTSDEN	(1 << 5)	/* RTS delta interrupt enable */
#define UCR1_SNDBRK	(1 << 4)	/* Send break */
#define UCR1_TDMAEN	(1 << 3)	/* Transmitter ready DMA enable */
#define UCR1_UARTCLKEN	(1 << 2)	/* UART clock enabled */
#define UCR1_DOZE	(1 << 1)	/* Doze */
#define UCR1_UARTEN	(1 << 0)	/* UART enabled */
#define UCR2_ESCI	(1 << 15)	/* Escape seq interrupt enable */
#define UCR2_IRTS	(1 << 14)	/* Ignore RTS pin */
#define UCR2_CTSC	(1 << 13)	/* CTS pin control */
#define UCR2_CTS	(1 << 12)	/* Clear to send */
#define UCR2_ESCEN	(1 << 11)	/* Escape enable */
#define UCR2_PREN	(1 << 8)	/* Parity enable */
#define UCR2_PROE	(1 << 7)	/* Parity odd/even */
#define UCR2_STPB	(1 << 6)	/* Stop */
#define UCR2_WS		(1 << 5)	/* Word size */
#define UCR2_RTSEN	(1 << 4)	/* Request to send interrupt enable */
#define UCR2_TXEN	(1 << 2)	/* Transmitter enabled */
#define UCR2_RXEN	(1 << 1)	/* Receiver enabled */
#define UCR2_SRST	(1 << 0)	/* SW reset */
#define UCR3_DTREN	(1 << 13)	/* DTR interrupt enable */
#define UCR3_PARERREN	(1 << 12)	/* Parity enable */
#define UCR3_FRAERREN	(1 << 11)	/* Frame error interrupt enable */
#define UCR3_DSR	(1 << 10)	/* Data set ready */
#define UCR3_DCD	(1 << 9)	/* Data carrier detect */
#define UCR3_RI		(1 << 8)	/* Ring indicator */
#define UCR3_ADNIMP	(1 << 7)	/* Autobaud Detection Not Improved */
#define UCR3_RXDSEN	(1 << 6)	/* Receive status interrupt enable */
#define UCR3_AIRINTEN	(1 << 5)	/* Async IR wake interrupt enable */
#define UCR3_AWAKEN	(1 << 4)	/* Async wake interrupt enable */
#define UCR3_DTRDEN	(1 << 3)
#define UCR3_RXDMUXSEL	(1 << 2)
#define UCR3_INVT	(1 << 1)	/* Inverted Infrared transmission */
#define UCR3_BPEN	(1 << 0)	/* Preset registers enable */
#define UCR4_CTSTL_32	(32 << 10)	/* CTS trigger level (32 chars) */
#define UCR4_INVR	(1 << 9)	/* Inverted infrared reception */
#define UCR4_ENIRI	(1 << 8)	/* Serial infrared interrupt enable */
#define UCR4_WKEN	(1 << 7)	/* Wake interrupt enable */
#define UCR4_REF16	(1 << 6)	/* Ref freq 16 MHz */
#define UCR4_IRSC	(1 << 5)	/* IR special case */
#define UCR4_TCEN	(1 << 3)	/* Transmit complete interrupt enable */
#define UCR4_BKEN	(1 << 2)	/* Break condition interrupt enable */
#define UCR4_OREN	(1 << 1)	/* Receiver overrun interrupt enable */
#define UCR4_DREN	(1 << 0)	/* Recv data ready interrupt enable */
#define UFCR_RXTL_SHF	0		/* Receiver trigger level shift */
#define UFCR_RFDIV	(7 << 7)	/* Reference freq divider mask */
#define UFCR_RFDIV_SHF	7		/* Reference freq divider shift */
#define RFDIV	 	4		/* divide input clock by 2 */
#define UFCR_DCEDTE	(1 << 6)	/* DTE mode select */
#define UFCR_TXTL_SHF	10		/* Transmitter trigger level shift */
#define USR1_PARITYERR	(1 << 15)	/* Parity error interrupt flag */
#define USR1_RTSS	(1 << 14)	/* RTS pin status */
#define USR1_TRDY	(1 << 13)	/* Transmitter ready interrupt/dma flag */
#define USR1_RTSD	(1 << 12)	/* RTS delta */
#define USR1_ESCF	(1 << 11)	/* Escape seq interrupt flag */
#define USR1_FRAMERR	(1 << 10)	/* Frame error interrupt flag */
#define USR1_RRDY	(1 << 9)	/* Receiver ready interrupt/dma flag */
#define USR1_TIMEOUT	(1 << 7)	/* Receive timeout interrupt status */
#define USR1_RXDS	(1 << 6)	/* Receiver idle interrupt flag */
#define USR1_AIRINT	(1 << 5)	/* Async IR wake interrupt flag */
#define USR1_AWAKE	(1 << 4)	/* Aysnc wake interrupt flag */
#define USR2_ADET	(1 << 15)	/* Auto baud rate detect complete */
#define USR2_TXFE	(1 << 14)	/* Transmit buffer FIFO empty */
#define USR2_DTRF	(1 << 13)	/* DTR edge interrupt flag */
#define USR2_IDLE	(1 << 12)	/* Idle condition */
#define USR2_IRINT	(1 << 8)	/* Serial infrared interrupt flag */
#define USR2_WAKE	(1 << 7)	/* Wake */
#define USR2_RTSF	(1 << 4)	/* RTS edge interrupt flag */
#define USR2_TXDC	(1 << 3)	/* Transmitter complete */
#define USR2_BRCD	(1 << 2)	/* Break condition */
#define USR2_ORE	(1 << 1)	/* Overrun error */
#define USR2_RDR	(1 << 0)	/* Recv data ready */
#define UTS_FRCPERR	(1 << 13)	/* Force parity error */
#define UTS_LOOP	(1 << 12)	/* Loop tx and rx */
#define UTS_TXEMPTY	(1 << 6)	/* TxFIFO empty */
#define UTS_RXEMPTY	(1 << 5)	/* RxFIFO empty */
#define UTS_TXFULL	(1 << 4)	/* TxFIFO full */
#define UTS_RXFULL	(1 << 3)	/* RxFIFO full */
#define UTS_SOFTRST	(1 << 0)	/* Software reset */
#define TXTL 		2		/* reset default */
#define RXTL 		1		/* reset default */

static vaddr_t chip_to_base(struct serial_chip *chip)
{
	struct imx_uart_data *pd =
		container_of(chip, struct imx_uart_data, chip);

	return io_pa_or_va(&pd->base, USIZE);
}

static void imx_uart_flush(struct serial_chip *chip)
{
	vaddr_t base = chip_to_base(chip);

	while (!(io_read32(base + UTS) & UTS_TXEMPTY))
		if (!(io_read32(base + UCR1) & UCR1_UARTEN))
			return;
}

static bool imx_uart_have_rx_data(struct serial_chip *chip)
{
	vaddr_t base = chip_to_base(chip);

	return io_read32(base + USR2) & USR2_RDR;
}

static int imx_uart_getchar(struct serial_chip *chip)
{
	vaddr_t base = chip_to_base(chip);

	while (io_read32(base + UTS) & UTS_RXEMPTY)
		;

	return (io_read32(base + URXD) & URXD_RX_DATA);
}

static void imx_uart_putc(struct serial_chip *chip, int ch)
{
	vaddr_t base = chip_to_base(chip);

	/* Wait until there's space in the TX FIFO */
	while (io_read32(base + UTS) & UTS_TXFULL)
		if (!(io_read32(base + UCR1) & UCR1_UARTEN))
			return;

	io_write32(base + UTXD, ch);
}

static const struct serial_ops imx_uart_ops = {
	.flush = imx_uart_flush,
	.getchar = imx_uart_getchar,
	.have_rx_data = imx_uart_have_rx_data,
	.putc = imx_uart_putc,
};
DECLARE_KEEP_PAGER(imx_uart_ops);

void imx_uart_init(struct imx_uart_data *pd, paddr_t pbase)
{
	vaddr_t base;
	uint32_t ucr1;

	pd->base.pa = pbase;
	pd->chip.ops = &imx_uart_ops;

	base = io_pa_or_va(&pd->base, USIZE);

	// Enabling interrupts.
	ucr1 = io_read32(base + UCR1);
	ucr1 = ucr1 | UCR1_RRDYEN;
	io_write32(base + UCR1, ucr1);
}

#ifdef CFG_DT
static struct serial_chip *imx_uart_dev_alloc(void)
{
	struct imx_uart_data *pd = calloc(1, sizeof(*pd));

	if (!pd)
		return NULL;

	return &pd->chip;
}

static int imx_uart_dev_init(struct serial_chip *chip, const void *fdt,
			     int offs, const char *parms)
{
	struct imx_uart_data *pd =
		container_of(chip, struct imx_uart_data, chip);
	vaddr_t vbase = 0;
	paddr_t pbase = 0;
	size_t size = 0;

	if (parms && parms[0])
		IMSG("imx_uart: device parameters ignored (%s)", parms);

	if (dt_map_dev(fdt, offs, &vbase, &size) < 0)
		return -1;

	pbase = virt_to_phys((void *)vbase);
	imx_uart_init(pd, pbase);

	return 0;
}

static void imx_uart_dev_free(struct serial_chip *chip)
{
	struct imx_uart_data *pd =
		container_of(chip, struct imx_uart_data, chip);

	free(pd);
}

static const struct serial_driver imx_uart_driver = {
	.dev_alloc = imx_uart_dev_alloc,
	.dev_init = imx_uart_dev_init,
	.dev_free = imx_uart_dev_free,
};

static const struct dt_device_match imx_match_table[] = {
	{ .compatible = "fsl,imx6q-uart" },
	{ 0 }
};

const struct dt_driver imx_dt_driver __dt_driver = {
	.name = "imx_uart",
	.match_table = imx_match_table,
	.driver = &imx_uart_driver,
};

#endif /* CFG_DT */

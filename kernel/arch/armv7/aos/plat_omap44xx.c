/*
 * Reference implementation of AOS milestone 0, on the Pandaboard.
 */

/*
 * Copyright (c) 2009-2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <kernel.h>

#include <assert.h>
#include <bitmacros.h>
#include <omap44xx_map.h>
#include <paging_kernel_arch.h>
#include <platform.h>
#include <serial.h>

#define UART_LSR 0x48020014
#define UART_THR 0x48020000

#define GPIO_OE      0x4A310134
#define GPIO_DATAOUT 0x4A31013C

#define MSG(format, ...) printk( LOG_NOTE, "OMAP44xx: "format, ## __VA_ARGS__ )

void blink_leds(void);

/* RAM starts at 2G (2 ** 31) on the Pandaboard */
lpaddr_t phys_memory_start= GEN_ADDR(31);

/*** Serial port ***/

unsigned serial_console_port= 2;

errval_t
serial_init(unsigned port, bool initialize_hw) {
    /* XXX - You'll need to implement this, but it's safe to ignore the
     * parameters. */

    return SYS_ERR_OK;
};

void
serial_putchar(unsigned port, char c) {
    /* XXX - You'll need to implement this, but it's safe to ignore the
     * port parameter. */
    
    /* Wait until UART is ready. */
    volatile unsigned short* pUART_LSR = (volatile unsigned short*) UART_LSR;
    while (!(*pUART_LSR & (1 << 5)));
    /* Write char to THR. */
    volatile char* pUART_THR = (volatile char*) UART_THR;
    *pUART_THR = c;
}

__attribute__((noreturn))
char
serial_getchar(unsigned port) {
    /* XXX - You only need to implement this if you're going for the extension
     * component. */

    panic("Unimplemented.\n");
}

/*** LED flashing ***/

__attribute__((noreturn))
void
blink_leds(void) {
    /* XXX - You'll need to implement this. */
    volatile unsigned* pOE = (volatile unsigned*) GPIO_OE;
    volatile unsigned* pDATAOUT = (volatile unsigned*) GPIO_DATAOUT;
    unsigned led_bit = 1 << 8;
    if (*pOE & led_bit) {
        /* OE bit needs to be cleared (set to 0) to actually activate output. */
        *pOE ^= led_bit;
    }
    while(1) {
        *pDATAOUT ^= led_bit;
        int i;
        /* Wait for a while before flashing LED again. */
        for (i = 0; i < 300000000; ++i);
    }
}

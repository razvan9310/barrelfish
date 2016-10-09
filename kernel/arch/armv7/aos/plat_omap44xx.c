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

#define MSG(format, ...) printk( LOG_NOTE, "OMAP44xx: "format, ## __VA_ARGS__ )
#define TX_FIFO_BIT 5
#define LED_BIT 8

volatile int * uart_lsr = (int *)0x48020014;
volatile int * uart_thr = (int *)0x48020000;

volatile int * out_enab = (int *)0x4A310134;
volatile int * dataout  = (int *)0x4A31013C;

volatile int counter = 0;

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
    int tx_fifo, mask = (1 << TX_FIFO_BIT);    

    do {
        tx_fifo = (*uart_lsr & mask) >> TX_FIFO_BIT;
    }
    while (tx_fifo != 1);

    *uart_thr = c;
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
    
    int i;
    int mask = 1 << LED_BIT;
        
    *out_enab &= ~(mask);
    
    while (1) {
        for (i = 0; i < 47600000; i++) {
            counter++;
        }
        *dataout ^= mask;
    } 
}

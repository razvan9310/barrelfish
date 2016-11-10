/**
 * \file
 * \brief Second hello world application
 */

/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, CAB F.78, Universitaetstr. 6, CH-8092 Zurich,
 * Attn: Systems Group.
 */


#include <stdio.h>

#include <aos/aos.h>

int main(int argc, char *argv[])
{
    printf("Goodbye :(\n");

    char* c = (char*) malloc(120 * 1024 * 1024);
    *c = 'B';
    sys_debug_flush_cache();
    *(c + 40 * 1024 * 1024) = 'Y';
    sys_debug_flush_cache();
    *(c + 80 * 1024 * 1024) = 'E';
    sys_debug_flush_cache();

    debug_printf("%c%c%c\n", *c, *(c + 40 * 1024 * 1024),
    		*(c + 80 * 1024 * 1024));

    return 0;
}

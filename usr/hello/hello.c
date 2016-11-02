/**
 * \file
 * \brief Hello world application
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
    printf("Hello, world! from THE numbawan original AOS homie waddup yo you know I'm THE BAWS\n");
    
    debug_printf("Dereferencing NULL\n");
    int *null_ptr = NULL;    
    *null_ptr = 13;
    sys_debug_flush_cache();

    debug_printf("Accessing non-heap memory at 0xFFFFAFFF\n");
    volatile char* c = (volatile char*) 0xFFFFAFFF;
    *c = 'D';
    sys_debug_flush_cache();

    debug_printf("Accessing (unmapped) memory at 1073807360\n");
    c = (volatile char*) 1073807360u;
    for (size_t i = 0; i < 2 * BASE_PAGE_SIZE; ++i) {
    	c[i] = i % 4 == 0 ? 'A' : (i % 4 == 1 ? 'B' : (i % 4 == 2 ? 'C' : 'D'));
    }
    sys_debug_flush_cache();
    for (size_t i = 0; i < 2 * BASE_PAGE_SIZE; ++i) {
    	printf("%c", c[i]);
    }
    printf("\n");


    return 0;
}

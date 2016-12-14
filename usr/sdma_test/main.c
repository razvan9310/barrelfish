#include <aos/aos.h>
#include <aos/waitset.h>
#include <omap_timer/timer.h>
#include <sdma/sdma_rpc.h>

int main(int argc, char** argv)
{
    // 1. Connect to SDMA driver.
	struct sdma_rpc rpc;
    struct waitset sdma_ws;
    waitset_init(&sdma_ws);
	errval_t err = sdma_rpc_init(&rpc, &sdma_ws);
	if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "sdma_rpc_init failed");
    }
    debug_printf("Connected to SDMA driver\n");

    // 2. Allocate src frame.
    size_t frame_size = 1024 * BASE_PAGE_SIZE;
    struct capref src;
    size_t retsize;
    err = frame_alloc(&src, frame_size, &retsize);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "frame_alloc src failed");
    }
    debug_printf("Alllocated src frame\n");

    // 3. Map src frame into vspace.
    void* src_buf;
    err = paging_map_frame(get_current_paging_state(), &src_buf, retsize, src,
            NULL, NULL);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "paging_map_frame src failed");
    }
    debug_printf("Mapped src frame into vspace at %p\n", src_buf);

    // 4. Write into src frame.
    size_t buf_size = frame_size;
    char* src_cbuf = (char*) src_buf;
    for (size_t i = 0; i < buf_size; ++i) {
        src_cbuf[i] = 'U';
    }
    sys_debug_flush_cache();
    debug_printf("Wrote into src frame...\n");

    // 5. Allocate dst frame.
    struct capref dst;
    err = frame_alloc(&dst, frame_size, &retsize);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "frame_alloc dst failed");
    }
    debug_printf("Alllocated dst frame\n");

    // 6. Map dst frame into vspace.
    void* dst_buf;
    err = paging_map_frame(get_current_paging_state(), &dst_buf, retsize, dst,
            NULL, NULL);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "paging_map_frame dst failed");
    }
    debug_printf("Mapped dst frame into vspace at %p\n", dst_buf);

    // 7. Perform SDMA memcpy RPC.
    size_t dst_offset = 0;
    size_t src_offset = 0;
    size_t num_mem_ops = 32;

    omap_timer_init();
    omap_timer_ctrl(true);

    buf_size = BASE_PAGE_SIZE;
    for (size_t j = 0; j < 4; ++j) {
        uint64_t timer_start = omap_timer_read();
        for (size_t i = 0; i < num_mem_ops; ++i) {
            err = sdma_rpc_memcpy(&rpc, dst, dst_offset, src, src_offset, buf_size);
            if (err_is_fail(err)) {
                USER_PANIC_ERR(err, "sdma_rpc_memcpy failed");
            }
            // debug_printf("SDMA memcpy request sent, awaiting post-copy ack...\n");

            // 8. Wait for SDMA response.
            err = sdma_rpc_wait_for_response(&rpc);
            if (err_is_fail(err)) {
                USER_PANIC_ERR(err, "sdma_rpc_wait_for_response failed");
            }
            // debug_printf("SDMA MEMCPY OK: %u\n", i + 1);
            // debug_printf("Got SDMA post-copy ack! Printing dst memory content:\n");
        }
        uint64_t timer_end = omap_timer_read();
        debug_printf("*** Time elapsed for %u SDMA ops of size %u: %llu\n",
                num_mem_ops, buf_size, timer_end - timer_start);
        buf_size *= 10;
    }

    

    // 9. Print from mapped dst frame.
    // char* dst_cbuf = (char*) dst_buf;
    // for (size_t i = 0; i < buf_size; ++i) {
    //     if (i % 128 == 0) {
    //         printf("\n");
    //     }
    //     printf("%c", dst_cbuf[i]);
    // }

    buf_size = BASE_PAGE_SIZE;
    for (size_t j = 0; j < 4; ++j) {
        uint64_t timer_start = omap_timer_read();
        for (size_t i = 0; i < num_mem_ops; ++i) {
            memcpy(dst_buf, src_buf, buf_size);
        }
        uint64_t timer_end = omap_timer_read();
        debug_printf("*** Time elapsed for %u standard memcpy ops of size %u: "
                "%llu\n", num_mem_ops, buf_size, timer_end - timer_start);
        buf_size *= 10;
    }
}
#include <aos/aos.h>
#include <sdma/sdma_rpc.h>

int main(int argc, char** argv)
{
    // 1. Connect to SDMA driver.
	struct sdma_rpc rpc;
	errval_t err = sdma_rpc_init(&rpc, get_default_waitset());
	if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "sdma_rpc_init failed");
    }
    debug_printf("Connected to SDMA driver\n");

    // 2. Allocate src frame.
    struct capref src;
    size_t retsize;
    err = frame_alloc(&src, BASE_PAGE_SIZE, &retsize);
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
    debug_printf("Mapped src frame into vspace at %p\n', src_buf");

    // 4. Write into src frame.
    size_t buf_size = 399;
    char* src_cbuf = (char*) src_buf;
    for (size_t i = 0; i < buf_size; ++i) {
        src_cbuf[i] = 'U';
    }
    sys_debug_flush_cache();
    debug_printf("Wrote into src frame...\n");

    // 5. Allocate dst frame.
    struct capref dst;
    err = frame_alloc(&dst, BASE_PAGE_SIZE, &retsize);
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
    debug_printf("Mapped dst frame into vspace at %p\n', dst_buf");

    // 7. Perform SDMA memcpy RPC.
    size_t dst_offset = 0;
    size_t src_offset = 0;
    for (size_t i = 0; i < 32; ++i) {
        err = sdma_rpc_memcpy(&rpc, dst, dst_offset, src, src_offset, buf_size);
        if (err_is_fail(err)) {
            USER_PANIC_ERR(err, "sdma_rpc_memcpy failed");
        }
        debug_printf("SDMA memcpy request sent, awaiting post-copy ack...\n");

        // 8. Wait for SDMA response.
        err = sdma_rpc_wait_for_response(&rpc);
        if (err_is_fail(err)) {
            USER_PANIC_ERR(err, "sdma_rpc_wait_for_response failed");
        }
        debug_printf("Got SDMA post-copy ack! Printing dst memory content:\n");
    }

    // 9. Print from mapped dst frame.
    char* dst_cbuf = (char*) dst_buf;
    for (size_t i = 0; i < 512; ++i) {
        if (i % 128 == 0) {
            printf("\n");
        }
        printf("%c", dst_cbuf[i]);
    }
    printf("\n");
}
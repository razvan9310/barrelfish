#include <aos/aos.h>
#include <aos/waitset.h>
#include <omap_timer/timer.h>
#include <sdma/sdma_rpc.h>

#define DEFAULT_BUF_SIZE 1099
#define MAGNITUDE_ORDERS 1

inline size_t pow(size_t base, size_t exp) {
    size_t res = 1;
    for (size_t i = 0; i < exp; ++i) {
        res *= base;
    }
    return res;
}

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

    // I. Memcpy: 2. Allocate src frame.
    size_t frame_size = 1024 * 10 * BASE_PAGE_SIZE;
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
        src_cbuf[i] = (char) (97 + i % 26);
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
    char* dst_cbuf = (char*) dst_buf;

    // 7. Perform SDMA memcpy RPC.
    size_t dst_offset = 0;
    size_t src_offset = 0;
    size_t num_mem_ops = 10;

    omap_timer_init();
    omap_timer_ctrl(true);

    buf_size = DEFAULT_BUF_SIZE;//BASE_PAGE_SIZE;
    for (size_t j = 0; j < MAGNITUDE_ORDERS; ++j) {
        for (size_t i = 0; i < num_mem_ops; ++i) {
            uint64_t timer_start = omap_timer_read();
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

            uint64_t timer_end = omap_timer_read();
            debug_printf("*** Time elapsed for 1 SDMA memcpy of size %u: %llu\n",
                    buf_size, timer_end - timer_start);
        }

        // Testing.
        for (size_t i = 0; i < buf_size; ++i) {
            if (dst_cbuf[i + dst_offset] != src_cbuf[i + src_offset]) {
                USER_PANIC("SDMA MEMCPY FAILED: character at %u is %c, should "
                        "be %c", i, dst_cbuf[i], src_cbuf[i]);
            }
        }

        buf_size *= 10;
    }

    buf_size = DEFAULT_BUF_SIZE;
    for (size_t j = 0; j < MAGNITUDE_ORDERS; ++j) {
        for (size_t i = 0; i < num_mem_ops; ++i) {
            uint64_t timer_start = omap_timer_read();
            memcpy(dst_buf, src_buf, buf_size);
            uint64_t timer_end = omap_timer_read();
            debug_printf("*** Time elapsed for 1 standard memcpy of size %u: "
                    "%llu\n", buf_size, timer_end - timer_start);
        }
        buf_size *= 10;
    }

    // II. Memset.
    char val = 'X';
    buf_size = DEFAULT_BUF_SIZE;//BASE_PAGE_SIZE;
    for (size_t j = 0; j < MAGNITUDE_ORDERS; ++j) {
        for (size_t i = 0; i < num_mem_ops; ++i) {
            uint64_t timer_start = omap_timer_read();
            err = sdma_rpc_memset(&rpc, dst, dst_offset, buf_size, val);
            if (err_is_fail(err)) {
                USER_PANIC_ERR(err, "sdma_rpc_memset failed");
            }
            // debug_printf("SDMA memcpy request sent, awaiting post-copy ack...\n");

            // 8. Wait for SDMA response.
            err = sdma_rpc_wait_for_response(&rpc);
            if (err_is_fail(err)) {
                USER_PANIC_ERR(err, "sdma_rpc_wait_for_response failed");
            }

            uint64_t timer_end = omap_timer_read();
            debug_printf("*** Time elapsed for 1 SDMA memset of size %u: %llu\n",
                    buf_size, timer_end - timer_start);
        }

        // Testing.
        for (size_t i = 0; i < buf_size; ++i) {
            if (dst_cbuf[i + dst_offset] != val) {
                USER_PANIC("SDMA MEMSET FAILED: character at %u is %c, should "
                        "be %c", i, dst_cbuf[i], val);
            }
        }

        buf_size *= 10;
    }

    buf_size = DEFAULT_BUF_SIZE;
    for (size_t j = 0; j < MAGNITUDE_ORDERS; ++j) {
        for (size_t i = 0; i < num_mem_ops; ++i) {
            uint64_t timer_start = omap_timer_read();
            memset(dst_buf, val, buf_size);
            uint64_t timer_end = omap_timer_read();
            debug_printf("*** Time elapsed for 1 standard memset of size %u: "
                    "%llu\n", buf_size, timer_end - timer_start);
        }
        buf_size *= 10;
    }

    // III. Rotate.
    size_t width = 5;
    size_t height = 3;
    uint32_t* src_uibuf = (uint32_t*) src_buf;
    for (size_t i = 0; i < height; ++i) {
        for (size_t j = 0; j < width; ++j) {
            src_uibuf[i * width + j + src_offset] = i * width + j;
        }
    }
    sys_debug_flush_cache();
    debug_printf("@@@@ Matrix before rotation (width=%u, height=%u):\n",
            width, height);
    for (size_t i = 0; i < width * height; ++i) {
        printf("%u ", src_uibuf[i + src_offset]);
    }
    printf("\n");

    err = sdma_rpc_rotate(&rpc, dst, dst_offset, src, src_offset, width, height);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "sdma_rpc_rotate failed");
    }
    err = sdma_rpc_wait_for_response(&rpc);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "sdma_rpc_wait_for_response failed");
    }

    debug_printf("@@@@ Matrix after rotation (width=%u, height=%u):\n",
            height, width);
    
    uint32_t* dst_uibuf = (uint32_t*) dst_buf;
    for (size_t i = 0; i < width * height; ++i) {
        printf("%u ", dst_uibuf[i + dst_offset]);
    }
    printf("\n");
}
#include <aos/aos.h>
#include <aos/paging.h>
#include <aos/waitset.h>

#include "sdma.h"

struct sdma_driver sd;

int main(int argc, char** argv)
{
	errval_t err = sdma_map_device(&sd);
	if (err_is_fail(err)) {
		USER_PANIC_ERR(err, "sdma_map_device failed");
	}
	sdma_initialize_driver(&sd);

	err = sdma_get_irq_cap(&sd);
	if (err_is_fail(err)) {
		USER_PANIC_ERR(err, "sdma_get_irq_cap failed");
	}

	err = sdma_setup_config(&sd);
	if (err_is_fail(err)) {
		USER_PANIC_ERR(err, "sdma_setup_config failed");
	}

	struct capref frame;
	size_t retsize;
	err = frame_alloc(&frame, BASE_PAGE_SIZE, &retsize);
	if (err_is_fail(err)) {
		USER_PANIC_ERR(err, "frame_alloc BASE_PAGE_SIZE");
	}

	err = paging_map_frame(get_current_paging_state(), &sd.buf, retsize, frame,
			NULL, NULL);
	if (err_is_fail(err)) {
		USER_PANIC_ERR(err, "paging_map_frame failed");
	}
	char* cbuf = (char*) sd.buf;
	for (size_t i = 0; i < 512; ++i) {
		cbuf[i] = 'G';
	}
	sys_debug_flush_cache();

	sdma_test_copy(&sd, &frame);

	// Loop waiting for SDMA interrupts.
	while(true) {
		err = event_dispatch(get_default_waitset());
		if (err_is_fail(err)) {
			USER_PANIC_ERR(err, "event_dispatch");
		}
	}

	return 0;
}
#include <aos/aos.h>
#include <aos/aos_rpc.h>
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

	err = sdma_setup_rpc_server(&sd);
	if (err_is_fail(err)) {
		USER_PANIC_ERR(err, "sdma_setup_rpc_server failed");
	}

	// Spawn sdma_test.
	domainid_t pid;
	err =  aos_rpc_process_spawn(get_init_rpc(), "sdma_test", 0, &pid);
    if (err_is_fail(err)) {
    	USER_PANIC_ERR(err, "aos_rpc_process_spawn failed");
    }
    debug_printf("Pid for sdma_test is %u\n", pid);

	// Loop waiting for SDMA interrupts.
	while(true) {
		err = event_dispatch(get_default_waitset());
		if (err_is_fail(err)) {
			USER_PANIC_ERR(err, "event_dispatch");
		}
	}

	return 0;
}
#include <aos/aos.h>

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

	return 0;
}
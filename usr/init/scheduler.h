

enum urpc_op_type { 
	REFILL, 
	CHANNEL 
} urpc_op_type;


enum rpc_status { 
	NEW, 
	ALLOCATING_CFRAME,
	WRITE_CFRAME,
	READ_SFRAME,
} rpc_status_type;

struct urpc_task {

	urpc_op_type op;
	struct client_state client;

	struct urpc_task* next;
	struct urpc_task* prev;

} urpc_task; 


struct rpc_task {



	struct client_state client;



	struct rpc_task* next;
	struct rpc_task* prev;

} rpc_task;


void add_urpc_task(urpc_task task);
urpc_task pop_urpc_task();

void add_rpc_task(rpc_task task);
urpc_task pop_rpc_task();


struct rpc_task* rpc_queue;
struct rpc_task* tail_rpc_queue;

struct urrpc_task* urpc_queue;
struct urrpc_task* tail_urpc_queue;


void process_rpc_task(struct rpc_task rpc);

void task_urpc();

void task_rpc();
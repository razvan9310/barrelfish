#include "scheduler->h"


struct rpc_task* rpc_queue;
struct rpc_task* tail_rpc_queue;

struct urrpc_task* urpc_queue;
struct urrpc_task* tail_urpc_queue;

void add_urpc_task(urpc_task* task) 
{
    if (urpc_queue == NULL) {
        urpc_queue = task;
        task->next  = NULL;
        task->prev  = NULL;
        tail_urpc_queue = urpc_queue;
        tail_urpc_queue->next = NULL;
        tail_urpc_queue->prev = NULL;
    } else {
        tail_urpc_queue = task;
        tail_urpc_queue->prev = tail_urpc_queue;
    }
}

urpc_task* pop_urpc_task()
{
    if (urpc_queue == NULL) {
        printf("Careful, URPC queue is NULL\n");
        return NULL;
    }
    urpc_task* task;
    if (urpc_queue->next == tail_urpc_queue) {
        task = urpc_queue;
        urpc_queue = tail_urpc_queue;
        urpc_queue->next = NULL;
        urpc_queue->prev = NULL;
        tail_urpc_queue->prev = NULL;
    } else {
        task = urpc_queue;
        urpc_queue = task->next;
        urpc_queue->prev = NULL;
    }
    return task;
}


void add_rpc_task(rpc_task* task)
{
    if (rpc_queue == NULL) {
        rpc_queue = task;
        task->next  = NULL;
        task->prev  = NULL;
        tail_rpc_queue = rpc_queue;
        tail_rpc_queue->next = NULL;
        tail_rpc_queue->prev = NULL;
    } else {
        tail_rpc_queue = task;
        tail_rpc_queue->prev = tail_rpc_queue;
    }
}
urpc_task pop_rpc_task()
{
    if (rpc_queue == NULL) {
        printf("Careful, rpc queue is NULL\n");
        return NULL;
    }
    rpc_task* task;
    if (rpc_queue->next == tail_rpc_queue) {
        task = rpc_queue;
        rpc_queue = tail_rpc_queue;
        rpc_queue->next = NULL;
        rpc_queue->prev = NULL;
        tail_rpc_queue->prev = NULL;
    } else {
        task = rpc_queue;
        rpc_queue = task->next;
        rpc_queue->prev = NULL;
    }
    return task;
}






void process_rpc_task(struct rpc_task rpc);

void task_urpc();

void task_rpc();


while (1) {
    task_urpc();
    task_rpc();
}


/*process_rpc_task(rpc_task) {
    if (exists_channel(rpc_task.client)) {
        perform_rpc();
    } else {
        urpc_queue.add(rpc_task.client);
    }
}

task_urpc() {

    //requests coming from other core
    if (urpc_request) {
        request = urpc_frame.read();
        response = process_urpc_request(request);
        urpc_frame.write(response);
    }

    //requests performed by you
    if (pending_task_ready) {
        process_pending_urpc_task();
    } else {
        return;
    }

    if (!urpc_queue.empty()) {
        task = urpc_queue.pop();
        //puts task into the urpc frame
        urpc_frame.write(task);
    }

}

task_rpc() {
    while (!waitset.empty()) {
        rpc_queue.add(waitset.pop);
    }
    while (i < rpc_task_threshold) {
        i++;
        process_rpc_task(rpc_queue.pop());
    }
}*/



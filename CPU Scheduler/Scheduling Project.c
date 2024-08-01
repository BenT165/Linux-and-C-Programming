/* This is the only file you will be editing.
 * - Copyright of Starter Code: Prof. Kevin Andrea, George Mason University.  All Rights Reserved
i9 * - Copyright of Student Code: You!  
 * - Restrictions on Student Code: Do not post your code on any public site (eg. Github).
 * -- Feel free to post your code on a PRIVATE Github and give interviewers access to it.
 * -- You are liable for the protection of your code from others.
 * - Date: Jan 2023
 */

/* Fill in your Name and GNumber in the following two comment fields
 * Name:Benjamin Tate
 * GNumber:01339075
 */

// System Includes
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <pthread.h>
#include <sched.h>
// Local Includes
#include "op_sched.h"
#include "vm_support.h"
#include "vm_process.h"

//flags to use throughout program
#define CRITICAL_FLAG   (1 << 31) 
#define LOW_FLAG        (1 << 30) 
#define READY_FLAG      (1 << 29)
#define DEFUNCT_FLAG    (1 << 28)

//flag used to modify 28 least significant bits of process state
#define STATE_FLAG 0x0FFFFFFF

//indicates a process is starving
#define MAX_AGE 5


//HELPER FUNCTION PROTOTYPES
int append_queue(Op_queue_s *queue, Op_process_s *process);
void set_state(Op_process_s* process, unsigned int flag);
void unset_state(Op_process_s* process, unsigned int flag);
int check_crit(Op_process_s* process);
int check_low(Op_process_s* process);
Op_queue_s *queue_create(Op_queue_s *queue);
Op_process_s *remove_from_front(Op_queue_s *queue);
Op_process_s *remove_process(Op_queue_s *queue, int position);
int first_crit_pos(Op_queue_s *queue);
int search_pid(Op_queue_s *queue, pid_t pid);
void dealloc_queue(Op_queue_s *queue);

/* HELPER to update the state of a process based 
 * by setting a specific pattern of state bits to be ON,
 * corresponding to an input flag
 */
void set_state_on(Op_process_s* process, unsigned int flag){

	if(process == NULL){
                return;
        }

	//sets bits that are ON in flag to be ON for process state
        process->state |= flag;
}

/* HELPER to update the state of a process
 * by setting a specific pattern of state bits to be OFF,
 * corresponding to an input flag
 *
 */
void unset_state(Op_process_s* process, unsigned int flag){

	 if(process == NULL){
                return;
        }

	//sets the bits that are ON in flag to be OFF for process state
        process->state &= ~flag;
}

/*
 * HELPER: returns status of critical bit for a process
 * 1 ON, 0 OFF
 */
int check_crit(Op_process_s* process){

	return (process->state & CRITICAL_FLAG)>>31;
}

/*
 * HELPER: returns status of low bit for a process
 * 1 ON, 0 OFF
 */
int check_low(Op_process_s* process){

        return (process->state & LOW_FLAG)>>30;
}

/*
 * HELPER
 * Dynamically allocates memory for a queue and
 * and returns a pointer to it or NULL for ERROR.
 */
Op_queue_s *queue_create(Op_queue_s *queue) {

	//dynamically allocate memory for queue
        queue = NULL;
        queue = malloc(sizeof(Op_queue_s));

        //return NULL for error allocating memory
        if(queue == NULL){
                return NULL;
        }

        //initialize queue fields 
        queue->head = NULL;
	queue->count = 0;
	
	//return pointer to queue
	return queue;
}	

/*
 * HELPER
 * Adds a process pointer to the end of the designated queue
 * return 0 for success, -1 for error
 */
int append_queue(Op_queue_s *queue, Op_process_s *process){
 //from this file
	
	//check if queue or process is NULL, return -1 for error
	if(queue == NULL || process == NULL){
		return -1;
	}

	//if queue is empty->update queue head
	if(op_get_count(queue) == 0){
		
		queue->head = process;
		process->next = NULL;
	}		
	
	//otherwise, loop through to end of queue and add process at end
	else{

		Op_process_s *walker = NULL;
		walker = queue->head;
		
		while(walker->next != NULL){
			walker = walker->next;
		}

		walker->next = process;
	}	

	queue->count++; //increment queue count and return 0 for success
	return 0;
}

/* HELPER
 * Removes and returns a pointer to the first process in the designated queue
 * or NULL if no process could be removed or queue is unitialized
 */
 Op_process_s *remove_from_front(Op_queue_s *queue){
	
	//return NULL if queue is empty or unitialized
	if(queue == NULL || op_get_count(queue) <= 0){
		return NULL;
	}

	
	//store pointer to first process
	Op_process_s *removed_process = NULL;	
	removed_process = queue->head;

	//update the queue head to point to the next process
	queue->head = queue->head->next;

	//decrement size of queue
	queue->count--;
	
	//set next to NULL to avoid unexpected values in other functions
	removed_process->next = NULL;
	removed_process->age = 0;

	return removed_process;
}

/*
 * HELPER
 * Removes and returns a pointer to the process at the given position (index)
 * in the designated queue.
 * Returns pointer to that process or NULL if no process could be removed.
 */
 Op_process_s *remove_process(Op_queue_s *queue, int position){
 
	//if queue is uninitialized or position is out of bounds for queue -> ERROR
	if(queue == NULL || position < 0 || position >= (op_get_count(queue))){
		return NULL;
	}

	//if process being removed is head, call helper
	else if(position == 0){
		return remove_from_front(queue);
	}

	//set up variables to track position in queue
	int current_position = 0;
	Op_process_s *walker = queue->head;

	//container for process being removed
	Op_process_s *removed_process = NULL;

	//iterate through queue until we find predecessor to process we want to remove or reach end of queue
	while(walker->next != NULL){

		if(current_position == position - 1){
		
			// Save and skip over process we are removing and decrement queue size.
			removed_process = walker->next;
			walker->next = walker->next->next;
			queue->count--;		

			//process is no longer pointing to anything or waiting to be processed
			removed_process->next = NULL;
			removed_process->age = 0;
			
			return removed_process;
		}

		//continue iterating and update increment queue position
		walker = walker->next;
		current_position++;
	}
	
	return removed_process;
}

/* HELPER
 * Retrieves the position (index) of the first critical process in the given queue.
 * Return -1 if the queue does not contain a critical process.
 * 
 */
int first_crit_pos(Op_queue_s *queue){

	if(queue == NULL){
		return -1;
	}

	int index = 0; //tracks position of first critical process in queue

	Op_process_s *walker = NULL;
	walker = queue->head;
	
	//iterate through queue until first critical process is found
	while(walker != NULL){
		
		//return index of first critical process
		if(check_crit(walker)){
			return index;
		}

		walker = walker->next;
		index++;
	}
	
	//if critical process not found, return -1
	return -1;
}

/* HELPER
 * Finds and retrieves position of process with matching pid.
 * Returns -1 for not found or error.
 */
int search_pid(Op_queue_s *queue, pid_t pid){

	//check for problematic input
	if(queue == NULL || op_get_count(queue)<= 0){
		return -1;
	}

	//the position of process with designated pid given queue
	unsigned int index = 0;
 
        //create traversal pointer
        Op_process_s *walker = NULL;
        walker = queue->head;
        
	//iterate through queue until process with matching pid is found
        while(walker != NULL){
                 
        	//return critical process index
                if(walker->pid == pid){
                        return index;
                }
                 
                walker = walker->next;
                index++;
        }
        
	//if matching pid not found 
        return -1;	
}

/*
 * Deallocates the contents of a queue.
 */
void dealloc_queue(Op_queue_s *queue){

	if(queue == NULL){
		return;
	}

	Op_process_s *walker = NULL; //copy of the head
	Op_process_s *dead_process = NULL; //process being freed

	walker = queue->head;

	while(walker != NULL){

		free(walker->cmd); //free command of dead_process
		walker->cmd = NULL; // avoid dangling pointer
		dead_process = walker; //save pointer to dead process
		walker= walker->next; //go to next node
		free(dead_process); //free dead_process
	}

	//free(queue->head); //free memory for queue head
	queue->head = NULL;//make sure there's no dangling pointer
	
	free(queue); //free memory for queue
	queue = NULL; //set memory address of freed queue to be NULL
}


/*
 * Dynamically allocates memory for a schedule and its queues.
 * Return a pointer to the schedule created or NULL for error.
 */
Op_schedule_s *op_create() {

	//make sched point to null (override garbage value)	
	Op_schedule_s *sched = NULL;

	//dynamically allocate memory for schedule
	sched = malloc(sizeof(Op_schedule_s));

	//NULL return -> ERROR
	if(sched == NULL){
		return NULL;
	}
	
	//dynamically allocate memory for high queue
	sched->ready_queue_high = queue_create(sched->ready_queue_high);
        if(sched->ready_queue_high == NULL){
                return NULL;
        }

	//dynamically allocate memory for low queue
	sched->ready_queue_low = queue_create(sched->ready_queue_low);
        if(sched->ready_queue_low == NULL){
                return NULL;
        }


	//dynamically allocate memory for defunct queue	
	sched->defunct_queue = queue_create(sched->defunct_queue);
	if(sched->defunct_queue == NULL){
		return NULL;
	} 

	return sched;
}


/*
 * dynamically allocate memory for a process and initialize its fields
 *	-dynamically allocate memory for the command
 *	-set ready bit to 1, defunct bit to 0
 *	-is_low and is_critical determine if low and critical bits should be on or off
 *	-remaining 28 bits are set to 0
 * return NULL for error
 */ 	
Op_process_s *op_new_process(char *command, pid_t pid, int is_low, int is_critical) {

	//return NULL for error if command is null
	if(command == NULL){
		return NULL;
	}
	
	//override garbage value of process we are creating with NULL
	Op_process_s *process = NULL;

	//dynamically allocate memory for the process
	process = malloc(sizeof(Op_process_s));
	
	//NULL malloc -> ERROR
	if(process == NULL) {
		return NULL;
	}		

	//dynamically allocate memory for command (strlen + 1 for NULL terminator)
	int cmd_length = strlen(command) + 1;
	process->cmd = malloc(sizeof(char) * cmd_length);
	
	//NULL malloc -> ERROR
	if(process->cmd == NULL){
		return NULL;
	}
	
	//copy over process command to process being created
	strncpy(process->cmd, command, cmd_length);  
		
	process->pid = pid; //initialize id to provided id
	process->age = 0; //initialize age to 0
	process->next = NULL; //next to NULL

	process->state = 0 | READY_FLAG; //initialize all state bits to be off except for ready bit	

	//return null for error is process if low and critical
	if(is_low && is_critical){
		return NULL;
	}

	//initialize low and critical bits to match function input
	else if(is_low){
		set_state_on(process, LOW_FLAG);
	}

	else if(is_critical){
		set_state_on(process, CRITICAL_FLAG);
	}
	
	return process; //return pointer to the process being created
}


/*
 * First initializes ready bit of process to 1 and defunct bit to 0 (without changing critical or low bits)
 * Then appends a process to the queue corresponding to its low priority bit (update queue head if necessary)
 *	- 1 = low queue
 * 	- 0 = high queue
 *
 * return 0 for success, -1 for error
 */
int op_add(Op_schedule_s *schedule, Op_process_s *process) {

	
	//S1: check for invalid arguments
	if(schedule == NULL || process == NULL) {
		return -1;
	}

	//S2: set ready bit ON, defunct bit OFF, next pointer to NULL
	set_state_on(process, READY_FLAG);
	unset_state(process, DEFUNCT_FLAG);
	process->next = NULL;
	
	/*
	* S3-4: check low bit to determine which
	* queue to add process to and add it to that queue
	*/
	if(check_low(process)){
		return append_queue(schedule->ready_queue_low, process);
	}

	return append_queue(schedule->ready_queue_high, process);
}


/*
 * Returns number of process in designated queue or -1 if queue is NULL
 */
int op_get_count(Op_queue_s *queue){


	if(queue == NULL){
		return -1;
	}

	return queue->count;
}


/*
 * Removes and returns pointer to the first critical process in the high queue.
 * If there are no critical processes, perform same actions on first process instead.
 * -removed processes have their ages set to 0 and next pointers set to NULL
 *
 * Return NULL if queue is empty.
 */
Op_process_s *op_select_high(Op_schedule_s *schedule){

	//check if schedule is NULL or queue is empty
	if(schedule == NULL || op_get_count(schedule->ready_queue_high) <= 0){
		return NULL;
	}
	
	//search for position of first critical process in high queue
	int critical_index = 0;	
	critical_index = first_crit_pos(schedule->ready_queue_high); 

	//critical process found -> remove first critical process 
	if(critical_index >= 0){

		return remove_process(schedule->ready_queue_high, critical_index);
	}

	//critical process not found -> remove first process in queue
	return remove_from_front(schedule->ready_queue_high);
}

/*
 * Removes and returns pointer to first process in the low queue.
 * Almost dentical to op_select_high but 
 * LOW QUEUE WILL NOT HAVE CRITICAL PROCESSES!
 *
 * Return NULL for error.
 */
Op_process_s *op_select_low(Op_schedule_s *schedule){

	if(schedule == NULL){
		return NULL;
	}
	
 	return remove_from_front(schedule->ready_queue_low);
}

/*
 * Increases ages of all processes in low queue by 1.
 * Any processes with ages 5 or greater are removed from low queue and appended to high queue.
 * -removed processes have their ages set to 0 and their next pointers set to NULL
 *
 * Return 0 for success, -1 for errors
 */
int op_promote_processes(Op_schedule_s *schedule){
	
        //check if schedule is NULL, low queue is empty, or high queue is NULL
        if(schedule == NULL || schedule->ready_queue_high == NULL){
                return -1;
        }
	
	//empty queue -> return 0
	else if(op_get_count(schedule->ready_queue_low) <= 0 ){
		return 0;
	}

	//set up pointer for queue traversal
	Op_process_s *walker = NULL;
	walker = schedule->ready_queue_low->head;

	int position = 0;//this variable tracks list index, similar to arrays
	
	//go through all processes in low queue
	while(walker != NULL){

		walker->age++; //increment age for each process

		/*if starving process is found -> promote it to high queue*/
		if(walker->age >= MAX_AGE) {
			
			walker = walker->next; //set walker for next iteration (so we don't lose )
			append_queue(schedule->ready_queue_high, remove_process(schedule->ready_queue_low, position)); //delete current process and promote it to high queue
		}
		
		//if starving process not found, increment position and iterate traditionally
		else{
			walker = walker->next;
			position++;
		}
	}
	
	return -1;
}

/*
 * Appends given process to defunct queue.
 * Set defunct bit to 1, ready bit to 0.
 * Set 28 least significant bits to match exit code.
 *
 * Return 0 on success, -1 on failure
 */
int op_exited(Op_schedule_s *schedule, Op_process_s *process, int exit_code){

        //check for NULL arguments -> return error
        if(schedule == NULL || process == NULL) {
                return -1;
        }

	set_state_on(process, DEFUNCT_FLAG); //set process to be defunct
	unset_state(process, READY_FLAG); //set process not to be ready

        int gotta_be_safe = exit_code & STATE_FLAG; //create a mask from exit code with the 4 most significant bits OFF to be safe(keep rest the same)
        process->state |= gotta_be_safe; //set state bits for exit code

        return append_queue(schedule->defunct_queue, process); //add process to end of defunct queue
}

/*
 * Finds process with matching ID in high or low queue and removes it from
 * that queue, then adds the process to the defunct queue.
 * Set Defunct bit ofprocess to 1, Ready bit to 0.
 * Set the 28 lsbs of the defunct process' to match the exit code 
 * 
 * Return 0 for sucess, -1 for failure (or if pid not found)
 */
int op_terminated(Op_schedule_s *schedule, pid_t pid, int exit_code) {

	//S1 check for unitialized schedule
	if(schedule == NULL){
		return -1;
	}

	//S2 search low queue and high queue for process with matching pid
	int high_queue_position = -1;
	int low_queue_position = -1;
	
	Op_process_s *terminated_process = NULL; //process being terminated

	//search queues to find position of process to terminated
	high_queue_position = search_pid(schedule->ready_queue_high, pid);
	low_queue_position = search_pid(schedule->ready_queue_low, pid);

	//search and remove terminated process from high queue
	if(high_queue_position >= 0){
		terminated_process = remove_process(schedule->ready_queue_high, high_queue_position);
	}
	//search and remove terminated process from low queue
	else if(low_queue_position >= 0){
		terminated_process = remove_process(schedule->ready_queue_low, low_queue_position);
	}

	//if process found, update the state and add to defunct
	if(terminated_process != NULL){

		set_state_on(terminated_process, DEFUNCT_FLAG); //set defunct flag on
		unset_state(terminated_process, READY_FLAG); //set ready flag off
		set_state_on(terminated_process, (exit_code & STATE_FLAG)); //set state to match 28 lsb of exit code
	}
	
	return append_queue(schedule->defunct_queue, terminated_process);	
}

/*
 * Free all dynamically allocate memory used by this program
 */
void op_deallocate(Op_schedule_s *schedule){


	//free contents of each queue
	dealloc_queue(schedule->ready_queue_low);
	dealloc_queue(schedule->ready_queue_high);
	dealloc_queue(schedule->defunct_queue);

	free(schedule);		
	schedule = NULL; //eliminate dangling pointer
}


/* This is the only file you should update and submit. */

/* Fill in your Name and GNumber in the following two comment fields
 * Name: Ben Tate
 * GNumber: G01339075
 */

#include <sys/wait.h>
#include "logging.h"
#include "anav.h"
#include "parse.h"
#include "util.h"

/* Constants */
#define DEBUG 1 /* You can set this to 0 to turn off the debug parse information */
#define STOP_SHELL  0
#define RUN_SHELL   1

#define WRITE_END 1 //writing end of pipe
#define READ_END 0 //reading end of pipe

#define WRITE 1 //for input file
#define READ 0 //for output file


typedef int task_id;

/*struct containing information about tasks*/
typedef struct task{

	task_id task_num; //unique identifier for task (number of tasks + 1 on intialization)
	pid_t pid; //OS defined identifier for process
	int exit_code; //information about terminated process
	unsigned int state; //current state of process (all tasks start in ready state)
	unsigned int type; //foreground or background
	char *cmd; //string containing name of process being run
	char **argv; //array of arguments factor this in
}task;

/*nodes for a singly linked list of tasks*/
typedef struct task_node{

	task *data; //maybe make this void
	struct task_node* next; //pointer to next node in the list;
}task_node;

/*contains information for task_list*/
typedef struct task_header{

	task_node* head; //pointer to first node in the list
	task_node* tail; //pointer to last node in the list
	unsigned int size; //number of items in the list
}task_header;


//global variables
task_header *task_manager = NULL; //pointer to linked list header
pid_t fg_pid = 0; //pid of the foreground task (0 if no foreground task)

//constructor functions
task_header *make_header(); 
task_node *make_task_node(char *cmd, char **argv);
task *make_task();
void free_task(task_node *nd);
void free_all();

//accessor functions for task_nodes
pid_t get_pid(task_node*);
task_id get_tasknum(task_node*); 
int get_exitcode(task_node*);
unsigned int get_state(task_node*); 
unsigned int get_type(task_node*);
char* get_cmd(task_node*);
char** get_argv(task_node*);

//mutator functions for task_nodes
int set_pid(task_node*, pid_t pid);
int set_tasknum(task_node*); 
int set_exitcode(task_node*, int exit_code);
int set_state(task_node*, unsigned int state);
int set_type(task_node*, unsigned int type); 
int set_cmd(task_node*, char *cmd);
int set_argv(task_node*, char **argv);

//functions for working with linked list
int add_node(task_node *new_node);
task_node *remove_node(task_id task_num);
task_node *search_node(task_id task_num);
task_node *search_pid(pid_t pid);
int size();

//built-in shell functions
void run_command(Instruction inst, char **argv, int *do_run_shell, char *cmd);
void run_quit(char *cmd, char **argv, int *do_run_shell);
void run_help();
int init_task(char **argv, char *cmd);
void run_list();
void run_purge(task_id task_num);
int exec_setup(task_node **nd, task_id task_num); //helps with preparing a task to be executed
void exec(Instruction inst, int type, char *cmd);
void exec_child(Instruction inst, char *cmd, task_node *nd, task_id task_num);
void exec_parent(task_node *nd, pid_t pid, int *status, int type);
int can_exec(task_node *nd); //checks if task can be executed (in list/ not busy) 
void run_exec(Instruction inst, char *cmd);
void run_bg(Instruction inst, char *cmd);
void run_pipe(Instruction inst, char *cmd);
void send_signal(task_id task_num, unsigned int signal, int log_num);
void run_kill(Instruction inst);
void run_suspend(Instruction inst);
void run_resume(Instruction inst);

//file io/redirect functions
int open_file(char *filename, task_id task_num, int read_write);
int handle_redirect(char *infile, char *outfile2, int task_num);

//functions for logging errors
int log_busy(task_node *nd);
int log_idle(task_node *nd);
void log_sigchld(task_node *nd, int *exitcode);

//signal handling functions
void summon_guru(); //initializes master signal handler
void install_custom_handler(int signal);
void signal_guru(int signal); //handles all signals
void fg_assassin(); //handles sigint
void signal_babysitter(); //handles sigchld
void signal_prison_warden(); //handles sigtstp

//signal blocking/unblocking functions
void block_all_signals();
void unblock_all_signals();  


/*****STRUCT INITIALIZATION/CLEANUP FUNCTIONS*****/

/* Dynamic allocates memory for and returns a pointer to a header struct
 * for a linked list*/
task_header *make_header(){

	task_header* task_manager = calloc(1, sizeof(task_header));
	return task_manager; 
}

/* Dynamically allocates memory for and returns pointer to linked list
 * node containing task struct as data portion. Return pointer to node.*/
task_node *make_task_node(char *cmd, char **argv){
	
	//dynamically allocate memory for task node
	task_node *task_nd = calloc(1, sizeof(task_node));

	//return NULL for error allocating memory
	if(task_nd == NULL){
		return NULL;
	}
	
	//call helper to make task
	task* new_task = make_task(); 
	
	//return NULL for error allocating memory for task
	if(new_task == NULL){
		return NULL;
	}	

    //initialize node data
	task_nd->data = new_task;

	//initialize task fields
	set_cmd(task_nd, cmd);
	set_argv(task_nd, argv);
	set_tasknum(task_nd); 
    set_type(task_nd, LOG_BG);

	return task_nd; //return pointer to task_node
}

/*Dynamically allocates memory for and returns pointer to a task*/
task *make_task() {
	
	task* new_task = calloc(1, sizeof(task));	
	return new_task;
}

/*Cleanup task node data*/
void free_task(task_node *nd) {

    free(get_cmd(nd));
    nd->data->cmd = NULL;
    free(get_argv(nd));
    nd->data->argv = NULL;

    task_node *dead_node = nd;
    nd = nd->next;

    free(dead_node->data);
    nd->data = NULL;
    free(dead_node);
    dead_node = NULL;
}

/*Free all tasks in task manager and task manager*/
void free_all() {

    task_node *walker = task_manager->head;
    task_node *dead_task = NULL;

    while(walker != NULL) {

        free(get_cmd(walker)); //free command
        walker->data->cmd = NULL;
        free(get_argv(walker)); //free argv
        walker->data->argv = NULL;
        dead_task = walker; //iterate
        walker = walker->next;
        free(dead_task->data); //free dead data
        dead_task->data = NULL;
        free(dead_task);
        dead_task = NULL; //free dead node
        
    }

    task_manager->head = NULL;

    free(task_manager);
    task_manager = NULL;

}


/*****TASK ACCESSOR FUNCTIONS*****/

/*Retrieve pid from a node*/
pid_t get_pid(task_node *nd) {
	
    return nd->data->pid;
}

/*Retrieve task number from a node*/
task_id get_tasknum(task_node *nd) {
	
    return nd->data->task_num;
}

 /*Retrieve exit code from a node*/
int get_exitcode(task_node *nd) {
	
    return nd->data->exit_code;
}

/*Retrieve state from a node*/
unsigned int get_state(task_node *nd) {
	
    return nd->data->state;
}

/*Retrieves whether task node is a foreground or background process*/
unsigned int get_type(task_node* nd){
	
    return nd->data->type;
}

/*Retrieve command from a task_node*/
char* get_cmd(task_node *nd) {
	
    return nd->data->cmd;
}

/*Retrieve argv from a task_node*/
char** get_argv(task_node *nd) {
	
    return nd->data->argv;
}


/*****TASK MUTATOR FUNCTIONS*****/

/* Sets pid for a task_node.
 * Return 1 for success, 0 for failure.*/
int set_pid(task_node *nd, pid_t pid) {

	if(nd == NULL || nd->data == NULL){
		return 0;
	}

	nd->data->pid = pid;
	return 1;
}

/* Sets task number for a task_node.
 * Return 1 for success, 0 for failure.*/
int set_tasknum(task_node *nd) {

	if(nd == NULL || nd->data == NULL){
		return 0;
	}

	//set task number to be one greater than greatest task number in the list or 1 if empty	
	nd->data->task_num = size() == 0 ? 1 : get_tasknum(task_manager->tail) + 1;
	return 1;
}

/* Sets exit code for a task_node.
 * Return 1 for success, 0 for failure.*/
int set_exitcode(task_node *nd, int exit_code) {
	
	if(nd == NULL || nd->data == NULL){
		return 0;
	}

	nd->data->exit_code = exit_code;
	return 1;
}

/* Sets state for a task_node.
 * Return 1 for success, 0 for failure.*/
int set_state(task_node *nd, unsigned int state){

	if(nd == NULL || nd->data == NULL){
		return 0;
	}

	nd->data->state = state;
	return 1;
}

/*Sets node type to be either foreground or background.*/
int set_type(task_node* nd, unsigned int type){

	if(nd == NULL || nd->data == NULL){
		return 0;
	}

	nd->data->type = type;
	return 1;
}

/* Sets command for a task_node.
 * Return 1 for success, 0 for failure.*/
int set_cmd(task_node *nd, char *cmd){
	
	if(nd == NULL || nd->data == NULL){
		return 0;
	}

    //call util function to copy string over to node
	char *new_cmd = string_copy(cmd); 
	
	if(new_cmd == NULL){
		return 0;
	}
	
	nd->data->cmd = new_cmd;
    return 1;
}

/* Sets argv for a task_node.
 * Return 1 for success, 0 for failure.*/
int set_argv(task_node *nd, char **argv){
	
	if(nd == NULL || nd->data == NULL){
		return 0;
	}

    //call util function to create copy of argv
	char **new_argv = clone_argv(argv);
	
	if(new_argv == NULL){
		return 0;
	}

	nd->data->argv = new_argv;
	return 1;
}


/*****LINKED LIST UTILITY FUNCTIONS*****/

/*
 * Adds a task node to the task_manager in position based on its task number.
 * Return 1 for operation success, 0 for for operation failure.
 */
int add_node(task_node *new_node) {

	//operation fails is new_node is NULL
	if(new_node == NULL){
		return 0;
	}

	//if task list is empty, update task manager head and tail
	if(size() == 0){
		task_manager->head = new_node;
	}
	
	//otherwise, put new node at end and update tail
	else{
		new_node->next = task_manager->tail->next; 
		task_manager->tail->next = new_node; 
	}

    task_manager->tail = new_node; //have tail reference new node
	task_manager->size++; //increment list size
	return 1; //return 1 to indicate operation success
}

/* Removes a node with the designated task id from the list.
 * Return a pointer to the removed node or NULL if node 
 * could not be removed.*/
task_node* remove_node(task_id task_num){

	//return NULL if list is empty
	if(!task_manager || !size()){
        log_anav_task_num_error(task_num);
		return NULL;
	}

	//container for node being removed
	task_node *dead_node = NULL; 

	//otherwise if removed node is the head of the list -> update head
	if(task_num == get_tasknum(task_manager->head)){
        
         //log error if task is busy
        if(log_busy(task_manager->head)){    
            return NULL;
        }

		dead_node = task_manager->head; //save reference to head
		task_manager->head = task_manager->head->next; //update head

        if(task_manager->head && task_manager->head->next == NULL){
            task_manager->tail = task_manager->head; //update tail if necessary
        }

		task_manager->size--; //decrement size of list
		return dead_node; //return deleted node
	}

	//otherwise search for node to remove
	else{
		task_node *walker = task_manager->head; //traversal node

		while(walker->next != NULL){

			//if matching node found -> remove it
			if(get_tasknum(walker->next) == task_num){

                //log error if task is busy
                if(log_busy(walker->next)){    
                     return NULL;
                }

				dead_node = walker->next; //save reference to node being removed
				walker->next = walker->next->next; //skip over deleted node
		
				if(walker->next == NULL){
					task_manager->tail = walker; //update tail if necessary
				}

				task_manager->size--; //decrement size
				return dead_node; //return deleted node
			}
		
			walker = walker->next; //continue iterating through task manager
		}
	}

    //log error for task not found and return NULL if task not found
    log_anav_task_num_error(task_num);  
	return NULL; 
}

/* Searches for and returns pointer to node in the task mangager
 * or NULL if not found*/
task_node *search_node(task_id task_num){

	task_node *walker = task_manager->head; //set up traversal node

	while(walker != NULL){

		//return pointer to node with matching task number
		if(get_tasknum(walker) == task_num){
			return walker;
		}

        walker = walker->next;
	}

    //log error and return NULL if task not found
    log_anav_task_num_error(task_num); 
	return NULL;
}

/*Searches for a node based on its process id*/
task_node *search_pid(pid_t pid){

	task_node *walker = task_manager->head; //traversal node

	while(walker != NULL){

		//return pointer to node with matching task number
		if(get_pid(walker) == pid){
			return walker;
		}

        walker = walker->next; //iterate through list
	}
 
	return NULL; //return NULL if task not found
}

/*return number of task in task manager or -1 if task list is NULL*/
int size(){
	return task_manager->size;
}


/*****BUILT IN SHELL FUNCTIONS/BUILT-IN RELATED FUNCTIONS*****/

/*Runs a built in command or adds task to task manager if task is not built in*/
void run_command(Instruction inst, char **argv, int *do_run_shell, char *cmd){

    if(!inst.instruct) {
        return;
    }

    else if(strncmp(inst.instruct, "quit", 4) == 0) {
	    run_quit(inst.instruct, argv, do_run_shell);
    }
    else if(!strncmp(inst.instruct, "help", 4)){
        run_help();
	}
	else if(!strncmp(inst.instruct, "list", 4)){
        run_list();
	}
	else if(!strncmp(inst.instruct, "purge", 5)){
		run_purge(inst.id1);
	}	
	else if(!strncmp(inst.instruct, "exec", 4)){
		run_exec(inst, cmd);
	}
	else if(!strncmp(inst.instruct, "bg", 2)){
        run_bg(inst, cmd);
	}
	else if(!strncmp(inst.instruct, "pipe", 4)){
		run_pipe(inst, cmd);
	}	
	else if(!strncmp(inst.instruct, "kill", 4)){
		run_kill(inst);
	}	
	else if(!strncmp(inst.instruct, "suspend", 7)){
		run_suspend(inst);
	}
	else if(!strncmp(inst.instruct, "resume", 6)){
		run_resume(inst);
	}
    else { 
		init_task(argv, cmd);
    }
}

/*Quit shell and free memory*/
void run_quit(char *cmd, char **argv, int *do_run_shell){ 

    free_all();
    log_anav_quit(); //display message and quit
	
    /* This is a match, so we'll set the main loop to exit when you finish processing it */
	*do_run_shell = STOP_SHELL;
}

/*Display shell prompt*/
void run_help() {

	log_anav_help();
}

/*Displays all tasks in list*/
void run_list(){

    //if task_manager hasn't been created yet, log 0 tasks
    if(!task_manager){
        log_anav_num_tasks(0);
        return;
    }
  
    log_anav_num_tasks(size()); //log number of tasks in list

    //create temp node to walk through listi
    task_node *walker = task_manager->head;

    //iterate through list and log all tasks
    while(walker){
 
        log_anav_task_info(get_tasknum(walker),  get_state(walker), get_exitcode(walker), get_pid(walker), get_cmd(walker));
        walker = walker->next;
    }
}

/*Executes purge built-in instruction*/
void run_purge(task_id task_num){

    task_node *dead_node = NULL;

    block_all_signals();

    //log delete if successful
    if((dead_node = remove_node(task_num))){
        free(dead_node); //free dead node
        log_anav_purge(task_num);
    }

    unblock_all_signals();
}

/*Returns 1 if task exists AND is not busy or 0 if task does not exist OR is busy*/
int can_exec(task_node *nd){

    if(!nd || log_busy(nd)){
        return 0;
    }

    return 1;
}

/* Helps set up execution for parent before forking.
 * Searches list for task node and stores that task into nd.
 * Return 0 for error, 1 for success.
 */
int exec_setup(task_node **nd, task_id task_num){

    block_all_signals(); //block all signals while we search list
    *nd = search_node(task_num); //get task to exec
    set_exitcode(*nd, 0); //reset exit code to be 0 for process that will run
    unblock_all_signals();

    //if task cannot be executed, log error and return
    if(!can_exec(*nd) || strncmp(get_cmd(*nd), "vim", 3) == 0){
        return 0;
    }

    return 1;
}

/*Handles execution for a child process*/
void exec_child(Instruction inst, char *cmd, task_node *nd, task_id task_num) {
         
        char **argv = get_argv(nd); //get argv for the current task 
        unblock_all_signals(); //unblock all signals going to child  
       
        //exit if there is an error with redirection
        if(handle_redirect(inst.infile, inst.outfile, task_num) == -1){ 
            exit(1);
        }

        
        //try to execute local program
        char local_path[MAXLINE + 3] = "./";
        strncat(local_path, argv[0], MAXLINE); 
        execv(local_path, argv);     

        //try to execute linux program
        char linux_path[MAXLINE + 10] = "/usr/bin/";
        strncat(linux_path, argv[0], MAXLINE); 
        execv(linux_path, argv);
   
        //if command can't be executed, log error and exit
        log_anav_exec_error(get_cmd(nd));
        exit(1);
}

/*Handles parent execution after forking*/
void exec_parent(task_node *nd, pid_t pid, int *status, int type){

    set_pid(nd, pid); //update pid for task
    if(get_type(nd) == LOG_FG){
        fg_pid = pid;
    }
    
    log_anav_status_change(get_tasknum(nd), get_pid(nd), get_type(nd), get_cmd(nd), LOG_START); //log status change for running process     
    unblock_all_signals();
}

/*Helper function for executing a process either in foreground or background*/
void exec(Instruction inst, int type, char *cmd) {

    task_node *nd = NULL; //task_node containing information about task being executed

    /*exit if setup fails (ie task busy or can't be found)*/
    if(!exec_setup(&nd, inst.id1)){
            return;    
    }

    set_type(nd, type); //set task to be running in either foreground or background
    set_state(nd, LOG_STATE_RUNNING); //set process to be running

    task_id task_num = inst.id1; //retrieve tasknum for current process
    pid_t current_pid = 0; //process id of current process running
    int status = 0;

    block_all_signals(); //block all signals while we search list
    current_pid = fork(); //fork to create child

    //print out an error message if fork fails and exit
    if(current_pid == -1){
        printf("Fork failed... exiting...");
        unblock_all_signals();
        exit(1);
    }

    //for child process
    if(!current_pid){
        setpgid(0, 0); //seperate child from parent's process group
        exec_child(inst, cmd, nd, task_num); //call helper to try to execute the child as a program
    }

    exec_parent(nd, current_pid, &status, type); //call helper to handle parent functionality
}

/*Executes task in the foreground*/
void run_exec(Instruction inst, char *cmd) {
 
    exec(inst, LOG_FG, cmd);
}

/*Executes task in the background*/
void run_bg(Instruction inst, char *cmd) {

    exec(inst, LOG_BG, cmd);
}

/*Creates pipe to redirect output from task1 to task2 */
void run_pipe(Instruction inst, char *cmd) {
 
    /*log error and return if tasks are the same*/
    if(inst.id1 == inst.id2){
        log_anav_pipe_error(inst.id1);
        return;
    }

    int fds[2]; //file descriptors for task1 and task2
    task_node *nd1, *nd2; //nodes containing each task
    task_id task_num1, task_num2; //task numbers for each task
    int status1, status2; //statuses for each task;
    pid_t pid1, pid2; //pid for each task
    
    nd1 = nd2 = NULL; //task nodes corresponding to task 1 and task 2
    pid1 = pid2 = 0; //process id of current process running
    status1 = status2 = 0; //initialize statuses

    /*exit if setup fails for task1 (ie task busy or can't be found)*/
    if(!exec_setup(&nd1, inst.id1)){
            return;    
    }
     
    /*exit if setup fails for task2 (ie task busy or can't be found)*/
    else if(!exec_setup(&nd2, inst.id2)){
            return;    
    }

    //create a pipe and log error if pipe creation fails
    else if(pipe(fds) < 0){
        log_anav_file_error(inst.id1, LOG_FILE_PIPE);
        return;
    }

    block_all_signals(); //block all signals before forking
    log_anav_pipe(inst.id1, inst.id2);
    set_type(nd1, LOG_BG); //set task 1 to be running in background
    set_state(nd1, LOG_STATE_RUNNING);  
    set_type(nd2, LOG_FG); //set task 2 to be running in foreground
    set_state(nd2, LOG_STATE_RUNNING); 
    
    //get task numbers for each task
    task_num1 = inst.id1;
    task_num2 = inst.id2;


    pid1 = fork(); //fork to create background child
 
    if(!pid1){

        setpgid(0,0); //put background child into its own process group
        close(fds[READ_END]); //close read end of pipe for background process
        dup2(fds[WRITE_END], STDOUT_FILENO);//redirect from STDOUT to write end of pipe (write to pipe)
        exec_child(inst, get_cmd(nd1), nd1, task_num1); //call helper to try to execute the child as a program
    }

    pid2 = fork(); //fork to create foreground child
    
    if(!pid2){
    
        setpgid(0,0); //put foreground child into its own process group
        close(fds[WRITE_END]); //close write end of pipe for foreground process
        dup2(fds[READ_END], STDIN_FILENO);//redirect from STDIN to read end of pipe (read from pipe)
        exec_child(inst, get_cmd(nd2), nd2, task_num2); //call helper to try to execute the child as a program
    }
 
    //parent closes both ends of pipe 
    close(fds[WRITE_END]);
    close(fds[READ_END]); 

    /*Parent updates status for both children to be running and unblocks signals.
     SIGCHLD handler (babysitter) takes care of reaping*/
    exec_parent(nd1, pid1, &status1, LOG_BG); 
    exec_parent(nd2, pid2, &status2, LOG_FG); 
    return;
}

/*Sends a SIGINT, SIGTSTP, or SIGCONT signals to a background process*/
void send_signal(task_id task_num, unsigned int signal, int log_num){

    pid_t bg_pid = 0;

    //block all signals and search list for task to interrupt
    block_all_signals();
    task_node *nd = search_node(task_num);
    
    //if task does not exist or is idle, don't interrupt
    if(!nd || log_idle(nd)) {
        unblock_all_signals();
        return;
    }
    
    bg_pid = get_pid(nd); //retrieve pid for child process we are sending signal to 
    log_anav_sig_sent(log_num, get_tasknum(nd), bg_pid); //log signal being sent   

    //if process gets kill signal while it is sleeping, wake it up
    if(signal == SIGINT && get_state(nd) == LOG_STATE_SUSPENDED){
        kill(get_pid(nd), SIGCONT);
        fg_pid = get_pid(nd);
    }
  
    kill(get_pid(nd), signal); //send signal to task
    unblock_all_signals(); //allow shell to receive signals again     
    install_custom_handler(signal); //reinstall custom action for SIGINT and SIGTSTP
}

/*Sends SIGINT to a specific background process*/
void run_kill(Instruction inst) {

    send_signal(inst.id1, SIGINT, LOG_CMD_KILL);
}

/*Sends SIGTSTP to a specific background process*/
void run_suspend(Instruction inst) {

    send_signal(inst.id1, SIGTSTP, LOG_CMD_SUSPEND);
}

/*Sends SIGTSTP to a specific background process*/
void run_resume(Instruction inst) {

    block_all_signals(); 
    task_node *nd = search_node(inst.id1); //search for task with provided id
    set_type(nd, LOG_FG); //set task to be running in foreground
    fg_pid = get_pid(nd); //set global pid to be pid of processing we are resuming
    unblock_all_signals(); //enable sigchld signals to be recieved for shell
    send_signal(inst.id1, SIGCONT, LOG_CMD_RESUME);
}

/* Creates a task and adds it to the task list
 * return 1 for success, 0 for failure 
 */
int init_task(char **argv, char *cmd){

	task_node *new_node = NULL;	
	new_node = make_task_node(cmd, argv); //create new task to add to the list

	/*return 0 for error creating node*/
	if(new_node == NULL){
		return 0;
	}

	log_anav_task_init(get_tasknum(new_node), cmd); //log info about task that was created
	return add_node(new_node); //add node to list	
}


/*****FILE IO FUNCTIONS *****/

/*Opens a file and returns its fd descriptor or -1 if file can't be opened*/
int open_file(char *filename, task_id task_num, int read_write) {

    int fd = -1;

    /*try to open input file with read only permissions*/
    if(read_write == READ){   
        fd = open(filename, O_RDONLY);
    }

    /* Try to open input file with write only permissions
     * cat onto it if it already exists, create it if it doesn't exist.
     */
    else if(read_write == WRITE){
        fd = open(filename, O_WRONLY | O_TRUNC | O_CREAT, 0644); 
    }

    /*log error if file couldn't be opened*/
    if(fd == -1){ 
        log_anav_file_error(task_num, filename);
    }

    return fd;
}

/* File redirect helper for exec: returns -1 for error, 0 for successful redirect.
 */
int handle_redirect(char *infile, char *outfile, int task_num) {

    int fd1, fd2;
    fd1 = fd2 = 0;
    
    /*if input file specified*/
    if(infile){

        //try to open input file and return -1 for error
        if((fd1 = open_file(infile, task_num, READ)) == -1){
            return -1;
        }

        //redirect input from STDIN to input file and log redirect
        log_anav_redir(task_num, LOG_REDIR_IN, infile);
        dup2(fd1, STDIN_FILENO);
    }
 
   //if output file specified
   if(outfile){

        //try to open output file and return -1 for error        
        if((fd2 = open_file(infile, task_num, WRITE)) == -1){

            //close input file if there is error opening output file
            if(fd1){
                close(fd1);
            }
            
            return -1;
        }

       //redirect from STDOUT to output file and log redirect
       log_anav_redir(task_num, LOG_REDIR_OUT, outfile);
       dup2(fd2, STDOUT_FILENO);
    }

    return 0;
} 


/*****LOGGING HELPER FUNCTIONS*****/

/*Return 1 and log busy message if task is busy, 0 if task is not busy*/
int log_busy(task_node *nd) {

    if(get_state(nd) == LOG_STATE_RUNNING || get_state(nd) == LOG_STATE_SUSPENDED){
    
        log_anav_status_error(get_tasknum(nd), get_state(nd));
        return 1;
    }
        
    return 0;
}

/*Return 1 and log idle message task is idle, 0 if task is not idle*/
int log_idle(task_node *nd) {

    if(get_state(nd) == LOG_STATE_READY || get_state(nd) == LOG_STATE_FINISHED || get_state(nd) == LOG_STATE_KILLED) {
    
        log_anav_status_error(get_tasknum(nd), get_state(nd));
        return 1;
    }
    
    return 0;
}

/*Logs messages for different sigchld signals. Signal babysitter helper.*/
void log_sigchld(task_node *nd, int *status) {

    //if child terminated by signal
    if(WIFSIGNALED(*status)){     
        
        set_exitcode(nd, WEXITSTATUS(*status)); //set exitcode if process has exited
        set_state(nd, LOG_STATE_KILLED); //set status to killed by a signal 
        log_anav_status_change(get_tasknum(nd), get_pid(nd), get_type(nd), get_cmd(nd), LOG_TERM_SIG); //log status change
        set_type(nd, LOG_BG);
        return;
    }

    //if child exited normally 
    else if(WIFEXITED(*status)){
     
        set_exitcode(nd, WEXITSTATUS(*status)); //set exitcode if process has exited
        set_state(nd, LOG_STATE_FINISHED); //set status to finished (3)
        log_anav_status_change(get_tasknum(nd), get_pid(nd), get_type(nd), get_cmd(nd), LOG_TERM); //log status change  
        set_type(nd, LOG_BG);
        return;
    }

    //if child suspended
    else if(WIFSTOPPED(*status)){
         
        set_state(nd, LOG_STATE_SUSPENDED); //set status to suspended
        log_anav_status_change(get_tasknum(nd), get_pid(nd), get_type(nd), get_cmd(nd), LOG_SUSPEND); //log status change 
        set_type(nd, LOG_BG);
        return; 
    }

    //if child resumed
    else if(WIFCONTINUED(*status)){
        set_state(nd, LOG_STATE_RUNNING); //set status to be running again
        set_type(nd, LOG_FG); //set task to be running in foreground
        log_anav_status_change(get_tasknum(nd), get_pid(nd), get_type(nd), get_cmd(nd), LOG_RESUME); //log status change
     
        /*wait for process in foreground*/
        if(waitpid(get_pid(nd), status, 0) != -1){ 
            log_sigchld(nd, status); //After process is reaped, re-call function to log message if it exited normally
        }
        return;  
    }

    return;
}


/*****SIGNAL HANDLING FUNCTIONS*****/

/*Initialize a signal handler for all signals*/
void summon_guru() {

    struct sigaction the_conductor = {0}; //initialize signal handler
    the_conductor.sa_handler = signal_guru;  //tell function we want it to call
    sigaction(SIGINT, &the_conductor, NULL);  //register signals with OS 
    sigaction(SIGCHLD, &the_conductor, NULL);  
    sigaction(SIGTSTP, &the_conductor, NULL);
}

/*Initialize signal handler with behavior for a given signal*/
void install_custom_handler(int signal) {

    if(signal == SIGCONT){
        return; //ignore SIGCONT
    }

    struct sigaction def_opts = {0};
    def_opts.sa_handler = signal_guru; //have OS perform custom action for signal
    sigaction(signal, &def_opts, NULL);  //register signal handler with OS 
}

/*An absolute baller. Handles all signals that require unique behavior (SIGINT, SIGCHLD, SIGTSTP) for this program*/
void signal_guru(int signal){

    if(signal == SIGINT){
        fg_assassin();
    }

    else if(signal == SIGCHLD){
        signal_babysitter();
    }

    else if(signal == SIGTSTP){
        signal_prison_warden();
    }
}

/*SIGINT handler. Logs message when signal is recieved and forwards SIGINT to foreground process.*/
void fg_assassin() {
 
    pid_t pid = 0;
    log_anav_ctrl_c(); //log ctrl c signal being sent
    task_node *nd = search_pid(fg_pid); //search list for foreground process 

    //if foreground process found
    if(nd) {        
        pid = get_pid(nd);
        kill(pid, SIGINT); //send SIGINT to child
    }
}

/*SIGCHLD handler. Reaps and sets exit code and status for child processes and logs updates.*/
void signal_babysitter() { 

    int status, fg_status;
    pid_t reaped_pid;
    task_node *nd, *fg_nd;
    nd = fg_nd = NULL;
 
    status = fg_status = 0;
    reaped_pid = 0;
   
    //wait until all children have exited
    do{
   
        //if foreground process exists, find node w/ matching pid and update its exitcode
        reaped_pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED); 
        nd = search_pid(reaped_pid);
        
        //if task found with non-zero pid -> update state/exit-code and log update to status
        if(nd && get_pid(nd)){

            log_sigchld(nd, &status); //call helper to log information about process
            
            if(fg_pid && fg_pid == reaped_pid){
                fg_pid = 0;
            }
        }

    }while(reaped_pid > 0);
   
   return;
}


/*SIGTSTP handler for foreground processes*/
void signal_prison_warden() {
    
    pid_t pid = 0;
    log_anav_ctrl_z(); //log ctrl signal being sent
    task_node *nd = search_pid(fg_pid); //search list for foreground process

    //if foreground process found
    if(nd) {
            
        pid = get_pid(nd);
        kill(pid, SIGTSTP); //forward SIGSTP to foreground process 
        fg_pid = 0;
        return;
    }
 
    return;
}


/*Blocks all signals*/
void block_all_signals() {

    sigset_t universal_mask, prev_mask;
    sigemptyset(&universal_mask); //initialize signal set to be empty
    sigfillset(&universal_mask); //add all signals to set
    sigprocmask(SIG_BLOCK, &universal_mask, &prev_mask); //add unblocking to mask for all signals
}  

/*Unblocks all signals*/
void unblock_all_signals() {

    sigset_t universal_mask, prev_mask;
    sigemptyset(&universal_mask); //initialize signal set to be empty
    sigfillset(&universal_mask); //add all signals to set
    sigprocmask(SIG_UNBLOCK, &universal_mask, &prev_mask); //add unblocking to mask for all signals
}  


/* The entry of your text processor program */
int main() {

    //allocate memory for and initialize task_manager (linked list header), exit if creation fails
    if((task_manager = make_header()) == NULL){
      exit(1);      
    }

    summon_guru(); //initializes master signal handler

	char *cmd = NULL;
	int do_run_shell = RUN_SHELL;

	/* Intial Prompt and Welcome */
	log_anav_intro();
  	log_anav_help();

  	/* Shell looping here to accept user command and execute */
  	while (do_run_shell == RUN_SHELL) {
    	char *argv[MAXARGS+1] = {0};  /* Argument list */
    	Instruction inst = {0};       /* Instruction structure: check parse.h */
  
        /*If there is no foreground process running, print prompt a get input*/
        if(!fg_pid){ 
   	        log_anav_prompt();
    	    cmd = get_input();
        }

        /* If the input is whitespace/invalid, get new input from the user. */
   	    if(cmd == NULL) {
      		continue;
    	}

    	/* Parse the Command and Populate the Instruction and Arguments */
    	initialize_command(&inst, argv);    /* initialize arg lists and instruction */
   	    parse(cmd, &inst, argv);            /* call provided parse() */

    	if (DEBUG) {  /* display parse result, redefine DEBUG to turn it off */
     		debug_print_parse(cmd, &inst, argv, "main (after parse)");
    	}

	    //handle user input
	    run_command(inst, argv, &do_run_shell, cmd);
        free_command(cmd, &inst, argv);
        cmd = NULL;
 }

 return 0;
}

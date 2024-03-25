#include "utils.h"

// Run the (executable, parameter) pairs in batches of 8 to avoid timeouts due to 
// having too many child processes running at once
#define PAIRS_BATCH_SIZE 8

typedef struct {
    char *executable_path;
    int parameter;
    int status;
} pairs_t;

// Store the pairs tested by this worker and the results
pairs_t *pairs;

// Information about the child processes and their results
pid_t *pids;
int *child_status;     // Contains status of child processes (-1 for done, 1 for still running)

int curr_batch_size;   // At most PAIRS_BATCH_SIZE (executable, parameter) pairs will be run at once
long worker_id;        // Used for sending/receiving messages from the message queue


// TODO: Timeout handler for alarm signal - should be the same as the one in autograder.c
void timeout_handler(int signum) {
    // Kill everything 
    for (int i = 0; i < curr_batch_size; ++i) {
        if (child_status[i] == 1 && pids[i] > 0) {
            kill(pids[i], SIGKILL);   
            child_status[i] = -1; 
        }
    } 
}


// Execute the student's executable using exec()
void execute_solution(char *executable_path, int param, int batch_idx) {
    pid_t pid = fork();

    // Child process
    if (pid == 0) {
        char *executable_name = get_exe_name(executable_path);

        // TODO: Redirect STDOUT to output/<executable>.<input> file
        char output_file[BUFSIZ];
        sprintf(output_file, "output/%s.%d", executable_name, param); // TODO: REPLACE "input" with message queue input
        int output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (output_fd == -1) {  
            perror("open");
            exit(1);
        }

        if (dup2(output_fd, STDOUT_FILENO) == -1) {
            perror("dup2");
            close(output_fd);
            exit(1);
        }

        // TODO: Input to child program can be handled as in the EXEC case (see template.c)
        char param_str[32];
        memset(param_str, 0, sizeof(param_str));
        sprintf(param_str, "%d", param);

        execlp(executable_path, executable_name, param_str, (char *) NULL);

        perror("Failed to execute program in worker");
        exit(1);
    }
    // Parent process
    else if (pid > 0) {
        pids[batch_idx] = pid;
    }
    // Fork failed
    else {
        perror("Failed to fork");
        exit(1);
    }
}


// Wait for the batch to finish and check results
void monitor_and_evaluate_solutions(int finished) {
    // Keep track of finished processes for alarm handler
    child_status = malloc(curr_batch_size * sizeof(int));
    for (int j = 0; j < curr_batch_size; j++) {
        child_status[j] = 1;
    }

    // MAIN EVALUATION LOOP: Wait until each process has finished or timed out
    for (int j = 0; j < curr_batch_size; j++) {
        // char *current_exe_path = pairs[finished + j].executable_path;
        // int current_param = pairs[finished + j].parameter;

        int status;
        pid_t pid = waitpid(pids[j], &status, 0);

        // TODO: What if waitpid is interrupted by a signal?
        while (pid == -1 && errno == EINTR) {
            pid = waitpid(pids[j], &status, 0);
        }

        // int exit_status = WEXITSTATUS(status);
        // int exited = WIFEXITED(status);
        int signaled = WIFSIGNALED(status);

        // TODO: Check if the process finished normally, segfaulted, or timed out and update the 
        //       pairs array with the results. Use the macros defined in the enum in utils.h for 
        //       the status field of the pairs_t struct (e.g. CORRECT, INCORRECT, SEGFAULT, etc.)
        //       This should be the same as the evaluation in autograder.c, just updating `pairs` 
        //       instead of `results`.
        
        // Programs either exit with a status or are killed by a signal
        if (signaled) {
            int signal_number = WTERMSIG(status);
            
            if (signal_number == SIGKILL) {
                // Child process was killed by the alarm
                pairs[finished + j].status = STUCK_OR_INFINITE;
            } else if (signal_number == SIGSEGV) {
                // Child process triggered a segmentation fault
                pairs[finished + j].status = SEGFAULT;
            }
        }

        // TODO: Also, update the results struct with the status of the child process
        char output_file[BUFSIZ];        
        char* filename = get_exe_name(pairs[finished + j].executable_path);
        
        sprintf(output_file, "output/%s.%d", filename, pairs[j].parameter);
        int output_fd = open(output_file, O_RDONLY);
        if (output_fd == -1) {  
            perror("open");
            exit(1);
        }
        
        // If file was written to, override pervious status
        char buffer[BUFSIZ];
        ssize_t num_bytes = read(output_fd, buffer, sizeof(buffer));
        if (num_bytes == -1) {
            perror("read");
            exit(1);
        }

        if (num_bytes != 0) {
            buffer[num_bytes] = '\0';  // Null-terminate the buffer
            pairs[finished + j].status = atoi(buffer) + 1; // Convert from 0/1 to 1/2 to match enum
        }
        close(output_fd);

        // Mark the process as finished
        child_status[j] = -1;
    }

    free(child_status);
}


// Send results for the current batch back to the autograder
void send_results(int msqid, long mtype, int finished) { 
    // Format of message should be ("%s %d %d", executable_path, parameter, status)
    for (int i = 0; i < curr_batch_size; i++) { 
        msgbuf_t msg;
        msg.mtype = mtype;
        sprintf(msg.mtext, "%s %d %d", pairs[i + finished].executable_path, pairs[i + finished].parameter, pairs[i + finished].status);

        if (msgsnd(msqid, &msg, sizeof(msg.mtext), 0) == -1) {
            perror("msgsnd failed to send results");
            exit(1);
        }
    }
}

// Send DONE message to autograder to indicate that the worker has finished testing
void send_done_msg(int msqid, long mtype) {
    msgbuf_t msg;
    msg.mtype = mtype;
    sprintf(msg.mtext, "%s", "DONE");
    if (msgsnd(msqid, &msg, sizeof(msgbuf_t) - sizeof(long), 0) == -1) {
        perror("failed to send done message");
        exit(1);
    }
}


int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <msqid> <worker_id>\n", argv[0]);
        return 1;
    }

    int msqid = atoi(argv[1]);
    worker_id = atoi(argv[2]);

    // TODO: Receive initial message from autograder specifying the number of (executable, parameter) 
    // pairs that the worker will test (should just be an integer in the message body). (mtype = worker_id)

    msgbuf_t msg;
    if (msgrcv(msqid, &msg, sizeof(msgbuf_t) - sizeof(long), worker_id, 0) == -1) {
        perror("couldn't get pairs");
        exit(1);
    }

    // TODO: Parse message and set up pairs_t array
    int pairs_to_test = atoi(msg.mtext);
    pairs = malloc(sizeof(pairs_t) * pairs_to_test);

    // TODO: Receive (executable, parameter) pairs from autograder and store them in pairs_t array.
    //       Messages will have the format ("%s %d", executable_path, parameter). (mtype = worker_id)
    for (int i = 0; i < pairs_to_test; ++i) {
        msgbuf_t exec_msg;
        if (msgrcv(msqid, &exec_msg, sizeof(msgbuf_t) - sizeof(long), worker_id, 0) == -1) {
            perror("failed to receive executable and parameter");
            exit(1);
        }
        
        char exec_path[BUFSIZ];
        memset(&exec_path, 0, sizeof(exec_path));

        if (sscanf(exec_msg.mtext, "%s %d", exec_path, &pairs[i].parameter) != 2) {
            write(STDERR_FILENO, "error receiving pair message\n", 30);
            exit(1);
        }

        pairs[i].executable_path = malloc(strlen(exec_path));
        strcpy(pairs[i].executable_path, exec_path);
        pairs[i].status = 0;
    }

    // TODO: Send ACK message to mq_autograder after all pairs received (mtype = BROADCAST_MTYPE)

    // TODO: Wait for SYNACK from autograder to start testing (mtype = BROADCAST_MTYPE).
    //       Be careful to account for the possibility of receiving ACK messages just sent.

    // Send ack to parent until it receives it (if we consumed an ack, resend it)
    msgbuf_t synacks;
    while (1) {
        msgbuf_t ack;
        memset(&ack, 0, sizeof(msgbuf_t));
        ack.mtype = BROADCAST_MTYPE;
        sprintf(ack.mtext, "%s", "ACK");

        if (msgsnd(msqid, &ack, sizeof(msgbuf_t) - sizeof(long), 0) == -1) {
            perror("failed to send acknowledgement"); 
            exit(1);
        }

        memset(&synacks, 0, sizeof(msgbuf_t));

        if (msgrcv(msqid, &synacks, sizeof(msgbuf_t) - sizeof(long), BROADCAST_MTYPE, 0) == -1) {
            perror("couldn't receive synack from autograder");
            exit(1);
        }
        
        if (strcmp(synacks.mtext, "SYNACK") == 0) {
            break;
        }
    }

    // Run the pairs in batches of 8 and send results back to autograder
    for (int i = 0; i < pairs_to_test; i+= PAIRS_BATCH_SIZE) {
        int remaining = pairs_to_test - i;
        curr_batch_size = remaining < PAIRS_BATCH_SIZE ? remaining : PAIRS_BATCH_SIZE;
        pids = malloc(curr_batch_size * sizeof(pid_t));

        for (int j = 0; j < curr_batch_size; j++) {
            // TODO: Execute the student executable
            execute_solution(pairs[i + j].executable_path, pairs[i + j].parameter, j);
        }

        // TODO: Setup timer to determine if child process is stuck
        start_timer(TIMEOUT_SECS, timeout_handler);  // Implement this function (src/utils.c)

        // TODO: Wait for the batch to finish and check results
        monitor_and_evaluate_solutions(i); // stuck in here

        // TODO: Cancel the timer if all child processes have finished
        if (child_status == NULL) {
            cancel_timer();
        } 

        // TODO: Send batch results (intermediate results) back to autograder
        send_results(msqid, worker_id, i);

        free(pids);
    }

    // TODO: Send DONE message to autograder to indicate that the worker has finished testing
    send_done_msg(msqid, worker_id);

    // Free the pairs_t array
    for (int i = 0; i < pairs_to_test; i++) {
        free(pairs[i].executable_path);
    }
    free(pairs);
}
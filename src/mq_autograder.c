#include "utils.h"

pid_t *workers;          // Workers determined by batch size
int *worker_done;        // 1 for done, 0 for still running

#define BUF_SIZE 20      // Defining buffer constant
// Stores the results of the autograder (see utils.h for details)
autograder_results_t *results;

int num_executables;      // Number of executables in test directory
int total_params;         // Total number of parameters to test - (argc - 2)
int num_workers;          // Number of workers to spawn


void launch_worker(int msqid, int pairs_per_worker, int worker_id) {
    pid_t pid = fork();

    // Child process
    if (pid == 0) {
        char str_msqid[BUF_SIZE];
        char str_workerid[BUF_SIZE];
        
        // Convert message queue id and worker id to strings to pass as command line args
        sprintf(str_msqid, "%d", msqid); 
        sprintf(str_workerid, "%d", worker_id);
        
        // TODO: exec() the worker program and pass it the message queue id and worker id.
        //       Use ./worker as the path to the worker program.
        execlp("./worker", "worker", str_msqid, str_workerid, NULL);
        perror("Failed to spawn worker");
        exit(1);
    } 
    else if (pid > 0) { // Parent process
        // TODO: Send the total number of pairs to worker via message queue (mtype = worker_id)
        msgbuf_t msg;
        msg.mtype = worker_id;
        sprintf(msg.mtext, "%d", pairs_per_worker);

        if (msgsnd(msqid, &msg, sizeof(msgbuf_t) - sizeof(long), 0) == -1) {
            perror("msgsnd");
            exit(1);
        }

        // Store the worker's pid for monitoring
        workers[worker_id - 1] = pid;
    }
    // Fork failed 
    else {
        perror("Failed to fork worker");
        exit(1);
    }
}


// TODO: Receive ACK from all workers using message queue (mtype = BROADCAST_MTYPE)
void receive_ack_from_workers(int msqid, int num_workers) {
    int acks = 0;
    // block until we receive an ack from each worker
    while (acks != num_workers) {
        msgbuf_t ack_msg;
        memset(&ack_msg, 0, sizeof(msgbuf_t));
        if (msgrcv(msqid, &ack_msg, sizeof(msgbuf_t) - sizeof(long), BROADCAST_MTYPE, 0) == -1) {
            perror("failed to receive ack from worker");
            exit(1);
        }
        if (strcmp(ack_msg.mtext, "ACK") == 0) {
            ++acks;
        }
    }
}


// TODO: Send SYNACK to all workers using message queue (mtype = BROADCAST_MTYPE)
void send_synack_to_workers(int msqid, int num_workers) {    
    msgbuf_t synack;
    memset(&synack, 0, sizeof(msgbuf_t));
    synack.mtype = BROADCAST_MTYPE;
    sprintf(synack.mtext, "%s", "SYNACK");

    // Loop through each worker to send them a SYNACK message
    for (int i = 0; i < num_workers; i ++) {
        if (msgsnd(msqid, &synack, sizeof(msgbuf_t) - sizeof(long), 0) == -1) {
            perror("failed to send synack");
            exit(1);
        }
    }
}

// Wait for all workers to finish and collect their resultsqueue
void wait_for_workers(int msqid, int pairs_to_test, char **argv_params) {
    int received = 0;
    worker_done = malloc(num_workers * sizeof(int));
    for (int i = 0; i < num_workers; i++) {
        worker_done[i] = 0;
    }
    
    // track number of times an executable has returned a result 
    // (executable idx same as results)
    // (don't assume num workers/cores proportional to num executables)
    int *param_stat_idx = malloc(sizeof(int) * num_executables);
    memset(param_stat_idx, 0, sizeof(int) * num_executables);

    while (received < pairs_to_test) {
        for (int i = 0; i < num_workers; i++) {
            if (worker_done[i] == 1) {
                continue;
            }

            // Check if worker has finished
            pid_t retpid = waitpid(workers[i], NULL, WNOHANG);

            int msgflg;
            if (retpid > 0) {
                // Worker has finished and still has messages to receive
                msgflg = 0;
            }
            else if (retpid == 0)
                // Worker is still running -> receive intermediate results
                msgflg = IPC_NOWAIT;
            else {
                // Error
                perror("Failed to wait for child process");
                exit(1);
            }

            // TODO: Receive results from worker and store them in the results struct.
            //       If message is "DONE", set worker_done[i] to 1 and break out of loop.
            //       Messages will have the format ("%s %d %d", executable_path, parameter, status)
            //       so consider using sscanf() to parse the message.
            while (1) {
                msgbuf_t worker_msg;
                char executable_path[BUF_SIZE];
                memset(executable_path, 0, sizeof(executable_path));
                int param;
                int status;

                if (msgrcv(msqid, &worker_msg, sizeof(msgbuf_t) - sizeof(long), i + 1, msgflg) == -1) {
                    sleep(1);
                    continue;
                }

                if (strcmp("DONE", worker_msg.mtext) == 0) {
                    worker_done[i] = 1;
                    break;
                }

                if (sscanf(worker_msg.mtext, "%s %d %d", executable_path, &param, &status) != 3) {
                    perror("failed to read worker's message");
                    exit(1);
                }

                // if (status > 4 || status < 1) {
                //     char param_msg[BUFSIZ];
                //     memset(param_msg, 0, BUFSIZ);
                //     sprintf(param_msg, "out of range status (%d) received for %s (%d)\n", status, executable_path, param);
                //     write(STDERR_FILENO, param_msg, strlen(param_msg));
                // }

                // Get executable record in results for writing
                int results_idx = 0;
                for (int i = 0; i < num_executables; ++i) {
                    if (strcmp(results[i].exe_path, executable_path) == 0) {
                        results_idx = i;
                    }
                }

                results[results_idx].params_tested[param_stat_idx[results_idx]] = param;
                results[results_idx].status[param_stat_idx[results_idx]] = status;
 
                ++received;
                ++param_stat_idx[results_idx];
            }
        }
    }
    free(param_stat_idx);
    free(worker_done);
}


int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <testdir> <p1> <p2> ... <pn>\n", argv[0]);
        return 1;
    }

    char *testdir = argv[1];
    total_params = argc - 2;

    char **executable_paths = get_student_executables(testdir, &num_executables);

    // Construct summary struct
    results = malloc(num_executables * sizeof(autograder_results_t));
    for (int i = 0; i < num_executables; i++) {
        results[i].exe_path = executable_paths[i];
        results[i].params_tested = malloc((total_params) * sizeof(int));
        results[i].status = malloc((total_params) * sizeof(int));
    }

    num_workers = get_batch_size();
    // Check if some workers won't be used -> don't spawn them
    if (num_workers > num_executables * total_params) {
        num_workers = num_executables * total_params;
    }
    workers = malloc(num_workers * sizeof(pid_t));

    // Create a unique key for message queue
    key_t key = IPC_PRIVATE;

    // TODO: Create a message queue
    int msqid;
    if ((msqid = msgget(key, IPC_CREAT | 0666)) == -1) {
        perror("msgget");
        exit(1);
    }

    int num_pairs_to_test = num_executables * total_params;
    
    // Spawn workers and send them the total number of (executable, parameter) pairs they will test
    for (int i = 0; i < num_workers; i++) {
        int leftover = num_pairs_to_test % num_workers - i > 0 ? 1 : 0;
        int pairs_per_worker = num_pairs_to_test / num_workers + leftover;

        // TODO: Spawn worker and send it the number of pairs it will test via message queue
        launch_worker(msqid, pairs_per_worker, i + 1);
    }

    // Send (executable, parameter) pairs to workers
    int sent = 0;
    for (int i = 0; i < total_params; i++) {
        for (int j = 0; j < num_executables; j++) {
            msgbuf_t msg;
            long worker_id = sent % num_workers + 1;
            msg.mtype = worker_id;
            results[j].params_tested[i] = atoi(argv[i + 2]);
            sprintf(msg.mtext, "%s %d", results[j].exe_path, results[j].params_tested[i]);

            // TODO: Send (executable, parameter) pair to worker via message queue (mtype = worker_id)
            if (msgsnd(msqid, &msg, sizeof(msgbuf_t) - sizeof(long), 0) == -1) {
                perror("msgsnd");
                exit(1);
            }
            sent++;
        }
    }

    // TODO: Wait for ACK from workers to tell all workers to start testing (synchronization)
    receive_ack_from_workers(msqid, num_workers);

    // TODO: Send message to workers to allow them to start testing
    send_synack_to_workers(msqid, num_workers);

    // TODO: Wait for all workers to finish and collect their results from message queue
    wait_for_workers(msqid, num_pairs_to_test, argv + 2);

    // TODO: Remove ALL output files (output/<executable>.<input>)
    for (int i = 2; i < argc; i++) {
        // we don't care about the batch size here, so we pass in num_executables twice
        remove_output_files(results, num_executables, num_executables, argv[i]); 
    }

    // sort results (order of parameters based on how the autograder
    // sends out work order as we wait/receive data from workers in 
    // sequential order which we don't control)
    if (total_params > 1) {
        for (int i = 0; i < num_executables; ++i) {
            for (int j = 1; j < total_params; ++j) {
                int key = results[i].params_tested[j];
                int key_status = results[i].status[j];
                int k = j - 1;
                while (k > -1 && results[i].params_tested[k] > key) {
                    results[i].params_tested[k + 1] = results[i].params_tested[k];
                    results[i].status[k + 1] = results[i].status[k];
                    --k; 
                }
                results[i].params_tested[k + 1] = key;
                results[i].status[k + 1] = key_status;
            }
        }
    }

    write_results_to_file(results, num_executables, total_params);

    // You can use this to debug your scores function
    // get_score("results.txt", results[0].exe_path);

    // Print each score to scores.txt
    write_scores_to_file(results, num_executables, "results.txt");

    // TODO: Remove the message queue
    if (msgctl(msqid, IPC_RMID, NULL) == -1) {
        perror("msgctl");
        exit(1);
    }

    // Free the results struct and its fields
    for (int i = 0; i < num_executables; i++) {
        free(results[i].exe_path);
        free(results[i].params_tested);
        free(results[i].status);
    }

    free(results);
    free(executable_paths);
    free(workers);
    
    return 0;
}
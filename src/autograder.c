#include "utils.h"

// Batch size is determined at runtime now
pid_t *pids;

// Stores the results of the autograder (see utils.h for details)
autograder_results_t *results;

int num_executables;      // Number of executables in test directory
int curr_batch_size;      // At most batch_size executables will be run at once
int total_params;         // Total number of parameters to test - (argc - 2)

// Contains status of child processes (-1 for done, 1 for still running)
int *child_status;

// TODO (Change 3): Timeout handler for alarm signal - kill remaining running child processes
void timeout_handler(int signum) {
    // Kill all child processes that are still running

    /*
    If you are failing the timeout_handler() test, make sure you are checking 
    if child_status[i] == 1 before calling kill() on pid[i]. If our tests, 
    pid[i] = -1 if child_status[i] == -1, so calling kill(pid[i], ...) 
    will result in errors since kill() will kill all processes available if 
    the first argument is -1.
    */

    for (int i = 0; i < curr_batch_size; ++i) {
        if (child_status[i] == 1 && pids[i] > 0) {
            kill(pids[i], SIGKILL);   
            child_status[i] = -1; 
        }
    } 
}

// Execute the student's executable using exec()
void execute_solution(char *executable_path, char *input, int batch_idx) {
    #ifdef PIPE
        // TODO: Setup pipe
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("couldn't create pipes");
            exit(1);
        }
    #endif
    
    pid_t pid = fork();

    // Child process
    if (pid == 0) {
        char *executable_name = get_exe_name(executable_path); // Get the executable name without the path
        // TODO (Change 1): Redirect STDOUT to output/<executable>.<input> file
        char output_file[BUFSIZ];
        sprintf(output_file, "output/%s.%s", executable_name, input); // Create the output file path
        int output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666); // Open the output file, create if it doesn't exist
        if (output_fd == -1) {
            perror("open");
            exit(1);
        }

        if (dup2(output_fd, STDOUT_FILENO) == -1) {
            perror("dup2");
            close(output_fd);
            exit(1);
        }

        // TODO (Change 2): Handle different cases for input source
        #ifdef EXEC
                    
        execlp(executable_path, executable_name, input, (char *) NULL); // Execute the program with the input parameter
        
        #elif REDIR
            
        // TODO: Redirect STDIN to input/<input>.in file

        char input_file[BUFSIZ];
        sprintf(input_file, "input/%s.in", input); // Create the input file path
        int input_fd = open(input_file, O_RDONLY | O_CREAT, 0666); // Open the input file, create if it doesn't exist

        if (input_fd == -1) {  
            perror("open");
            exit(1);
        }

        if (dup2(input_fd, STDIN_FILENO) == -1) { // Redirect STDIN to the input file
            perror("dup2");
            close(input_fd);
            exit(1);
        }
        
        execlp(executable_path, executable_name, (char *) NULL); // Execute the program without the input parameter

        #elif PIPE
            // TODO: Pass read end of pipe to child process 
        close(pipefd[1]); // close write end of pipe
        char fd_str[10];  // Buffer to hold the string representation of the file descriptor
        sprintf(fd_str, "%d", pipefd[0]);  // Convert the file descriptor to a string
        
        execlp(executable_path, executable_name, fd_str, (char *) NULL); // Execute the program with the pipe file descriptor
        
        #endif

        // If exec fails
        perror("Failed to execute program");
        exit(1);
    } 
    // Parent process
    else if (pid > 0) {
        #ifdef PIPE
            // TODO: Send input to child process via pipe 
            close(pipefd[0]);
            write(pipefd[1], input, strlen(input));
            close(pipefd[1]); // Signal EOF 

            // Store the PID for later
            pids[batch_idx] = pid;
            
        #endif

        pids[batch_idx] = pid;
    }
    // Fork failed
    else {
        perror("Failed to fork");
        exit(1);
    }
}


// Wait for the batch to finish and check results
void monitor_and_evaluate_solutions(int tested, char *param, int param_idx) {
    // Keep track of finished processes for alarm handler
    child_status = malloc(curr_batch_size * sizeof(int));
    for (int j = 0; j < curr_batch_size; j++) {
        child_status[j] = 1;
    }

    // MAIN EVALUATION LOOP: Wait until each process has finished or timed out
    for (int j = 0; j < curr_batch_size; j++) {

        int status;
        pid_t pid = waitpid(pids[j], &status, 0);

        // TODO: What if waitpid is interrupted by a signal?

        // Keep waiting for children while interrupted
        while (pid == -1 && errno == EINTR) {
            pid = waitpid(pids[j], &status, 0);
        }

        // TODO: Determine if the child process finished normally, segfaulted, or timed out
        int signaled = WIFSIGNALED(status);

        // Programs either exit with a status or are killed by a signal
        if (signaled) {
            int signal_number = WTERMSIG(status);
            
            if (signal_number == SIGKILL) {
                // Child process was killed by the alarm
                results[tested - curr_batch_size + j].status[param_idx] = STUCK_OR_INFINITE;
            } else if (signal_number == SIGSEGV) {
                // Child process triggered a segmentation fault
                results[tested - curr_batch_size + j].status[param_idx] = SEGFAULT;
            }
        }

        // TODO: Also, update the results struct with the status of the child process
        char output_file[BUFSIZ];
        char* filename = get_exe_name(results[tested - curr_batch_size + j].exe_path); // Get the executable name without the path
        sprintf(output_file, "output/%s.%s", filename, param); // Create the output file path
        int output_fd = open(output_file, O_RDONLY); // Open the output file
        if (output_fd == -1) {  
            perror("open");
            exit(1);
        }
        char buffer[BUFSIZ];
        ssize_t num_bytes = read(output_fd, buffer, sizeof(buffer)); // Read the output file
        if (num_bytes == -1) {
            perror("read");
            exit(1);
        }

        if (num_bytes != 0) {
            buffer[num_bytes] = '\0';  // Null-terminate the buffer
            results[tested - curr_batch_size + j].status[param_idx] = atoi(buffer) + 1; // Convert from 0/1 to 1/2 to match enum
        }
        close(output_fd);

        // NOTE: Make sure you are using the output/<executable>.<input> file to determine the status
        //       of the child process, NOT the exit status like in Project 1.

        // Adding tested parameter to results struct
        results[tested - curr_batch_size + j].params_tested[param_idx] = atoi(param);

        // Mark the process as finished
        child_status[j] = -1;
    }

    free(child_status);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <testdir> <p1> <p2> ... <pn>\n", argv[0]);
        return 1;
    }

    char *testdir = argv[1];
    total_params = argc - 2;

    // TODO (Change 0): Implement get_batch_size() function
    int batch_size = get_batch_size();

    char **executable_paths = get_student_executables(testdir, &num_executables);

    // Construct summary struct
    results = malloc(num_executables * sizeof(autograder_results_t));
    for (int i = 0; i < num_executables; i++) {
        results[i].exe_path = executable_paths[i];
        results[i].params_tested = malloc((total_params) * sizeof(int));
        results[i].status = malloc((total_params) * sizeof(int));
    }

    #ifdef REDIR
        // TODO: Create the input/<input>.in files and write the parameters to them
        create_input_files(argv + 2, total_params);  // Implement this function (src/utils.c)
        // fill file(s) with params in argv 
    #endif
    
    // MAIN LOOP: For each parameter, run all executables in batch size chunks
    for (int i = 2; i < argc; i++) {
        int remaining = num_executables;
	    int tested = 0;

        // Test the parameter on each executable
        while (remaining > 0) {

            // Determine current batch size - min(remaining, batch_size)
            curr_batch_size = remaining < batch_size ? remaining : batch_size;
            pids = malloc(curr_batch_size * sizeof(pid_t));
		
            // TODO: Execute the programs in batch size chunks
            for (int j = 0; j < curr_batch_size; j++) {
                execute_solution(executable_paths[tested], argv[i], j);
		        tested++;
            }

            // TODO (Change 3): Setup timer to determine if child process is stuck
            start_timer(TIMEOUT_SECS, timeout_handler);  // Implement this function (src/utils.c)

            // TODO: Wait for the batch to finish and check results
            monitor_and_evaluate_solutions(tested, argv[i], i - 2);

            // TODO: Cancel the timer if all child processes have finished
            if (child_status == NULL) {
                cancel_timer();  // Implement this function (src/utils.c)
            }

            // TODO Unlink all output files in current batch (output/<executable>.<input>)
            remove_output_files(results, tested, curr_batch_size, argv[i]);  // Implement this function (src/utils.c)

            // Adjust the remaining count after the batch has finished
            remaining -= curr_batch_size;
        }
    }

    #ifdef REDIR
        // TODO: Unlink all input files for REDIR case (<input>.in)
        remove_input_files(argv + 2, total_params);  // Implement this function (src/utils.c)
    #endif

    write_results_to_file(results, num_executables, total_params);

    // You can use this to debug your scores function
    // get_score("results.txt", results[0].exe_path);

    // Print each score to scores.txt
    write_scores_to_file(results, num_executables, "results.txt");

    // Free the results struct and its fields
    for (int i = 0; i < num_executables; i++) {
        free(results[i].exe_path);
        free(results[i].params_tested);
        free(results[i].status);
    }

    free(results);
    free(executable_paths);

    free(pids);
    
    return 0;
}
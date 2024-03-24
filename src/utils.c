#include "utils.h"


const char* get_status_message(int status) {
    switch (status) {
        case CORRECT: return "correct";
        case INCORRECT: return "incorrect";
        case SEGFAULT: return "crash";
        case STUCK_OR_INFINITE: return "stuck/inf";
        default: return "unknown";
    }
}


char *get_exe_name(char *path) {
    return strrchr(path, '/') + 1;
}


char **get_student_executables(char *solution_dir, int *num_executables) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;

    // Open the directory
    dir = opendir(solution_dir);
    if (!dir) {
        perror("Failed to open directory");
        exit(EXIT_FAILURE);
    }

    // Count the number of executables
    *num_executables = 0;
    while ((entry = readdir(dir)) != NULL) {
        // Ignore hidden files
        char path[PATH_MAX];
        sprintf(path, "%s/%s", solution_dir, entry->d_name);
        
        if (stat(path, &st) == 0) {
            if (S_ISREG(st.st_mode) && entry->d_name[0] != '.')
                (*num_executables)++;
        } 
        else {
            perror("Failed to get file status");
            exit(EXIT_FAILURE);
        }
    }

    // Allocate memory for the array of strings
    char **executables = (char **) malloc(*num_executables * sizeof(char *));

    // Reset the directory stream
    rewinddir(dir);

    // Read the file names
    int i = 0;
    while ((entry = readdir(dir)) != NULL) {
        // Ignore hidden files
        char path[PATH_MAX];
        sprintf(path, "%s/%s", solution_dir, entry->d_name);

        if (stat(path, &st) == 0) {
            if (S_ISREG(st.st_mode) && entry->d_name[0] != '.') {
                executables[i] = (char *) malloc((strlen(solution_dir) + strlen(entry->d_name) + 2) * sizeof(char));
                sprintf(executables[i], "%s/%s", solution_dir, entry->d_name);
                i++;
            }
        }
    }

    // Close the directory
    closedir(dir);

    // Return the array of strings (remember to free the memory later)
    return executables;
}


// TODO: Implement this function
int get_batch_size() {
    FILE *fp = fopen("/proc/cpuinfo", "r"); // Open the cpuinfo file
    if (fp == NULL) {
        perror("error opening cpuinfo");
        return -1; // Indicate failure
    }

    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), fp)) { // Read the file line by line
        if (strncmp(line, "processor", 9) == 0) { // Check for processors
            count++;
        }
    }

    fclose(fp);
    return count; // Return the count of "processor" occurrences
}


// TODO: Implement this function
void create_input_files(char **argv_params, int num_parameters) {
    for (int i = 0; i < num_parameters; ++i) {
        char buff[BUFSIZ];
        sprintf(buff, "input/%s.in", argv_params[i]); // Create the input file path
        int fd = open(buff, O_RDWR | O_CREAT, 0666); // Open the file, create if it doesn't exist

        if (fd == -1) {
            perror("error creating input files");
            exit(1);
        }

        if (write(fd, argv_params[i], strlen(argv_params[i])) == -1) { // Write the parameter to the file
            perror("error writing to input file");
            close(fd);
            exit(1);
    }
        close(fd);
    }
}

// TODO: Implement this function
void start_timer(int seconds, void (*timeout_handler)(int)) {
    // block all signals except alarm while handling it
    struct sigaction sa;
    sa.sa_handler = timeout_handler;
    sigfillset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    if (sigaction(SIGALRM, &sa, NULL) == -1) { // Set the signal handler for SIGALRM
        perror("sigaction");
        exit(1);
    }

    // set to send SIGALRM every TIMEOUT_SECS
    struct itimerval timer;
    timer.it_value.tv_sec = TIMEOUT_SECS;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;

    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) { // Set the timer to send SIGALRM every TIMEOUT_SECS
        perror("error setting timer");
        exit(1);
    }
}


// TODO: Implement this function
void cancel_timer() {
    struct itimerval timer; 
    memset(&timer, 0, sizeof(timer)); // Make timer struct 0 to cancel out ongoing timer
    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) { // Call setitimer with a 0 interval to stop the timer
        perror("error cancelling timer");
        exit(1);
    }
}


// TODO: Implement this function
void remove_input_files(char **argv_params, int num_parameters) {
    for (int i = 0; i < num_parameters; ++i) {
    char buff[BUFSIZ];
    sprintf(buff, "input/%s.in", argv_params[i]); // Create the input file path
        if (unlink(buff) == -1) { // Remove the input file
            perror("error removing input files");
            exit(1);
        }
    }
}


// TODO: Implement this function
void remove_output_files(autograder_results_t *results, int tested, int current_batch_size, char *param) {
    for (int i = (tested - current_batch_size); i < tested; i++) { // Remove the output files for the last batch
        char buff[BUFSIZ];
        sprintf(buff, "output/%s.%s", get_exe_name(results[i].exe_path), param); // Create the output file path
        if (unlink(buff) == -1) { // Remove the output file
            perror("error removing output files");
            exit(1);
        }
    }
}


int get_longest_len_executable(autograder_results_t *results, int num_executables) {
    int longest_len = 0;
    for (int i = 0; i < num_executables; i++) {
        char *exe_name = get_exe_name(results[i].exe_path);
        int len = strlen(exe_name);
        if (len > longest_len) {
            longest_len = len;
        }
    }
    return longest_len;
}
 

void write_results_to_file(autograder_results_t *results, int num_executables, int total_params) {
    FILE *file = fopen("results.txt", "w");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    // Find the longest executable name (for formatting purposes)
    int longest_len = 0;
    for (int i = 0; i < num_executables; i++) {
        char *exe_name = get_exe_name(results[i].exe_path);
        int len = strlen(exe_name);
        if (len > longest_len) {
            longest_len = len;
        }
    }

    // Sort the results data structure by executable name (specifically number at the end)
    for (int i = 0; i < num_executables; i++) {
        for (int j = i + 1; j < num_executables; j++) {
            char *exe_name_i = get_exe_name(results[i].exe_path);
            int num_i = atoi(strrchr(exe_name_i, '_') + 1);
            char *exe_name_j = get_exe_name(results[j].exe_path);
            int num_j = atoi(strrchr(exe_name_j, '_') + 1);
            if (num_i > num_j) {
                autograder_results_t temp = results[i];
                results[i] = results[j];
                results[j] = temp;
            }
        }
    }

    // Write results to file
    for (int i = 0; i < num_executables; i++) {
        char *exe_name = get_exe_name(results[i].exe_path);

        char format[20];
        sprintf(format, "%%-%ds:", longest_len);
        fprintf(file, format, exe_name); // Write the program path
        for (int j = 0; j < total_params; j++) {
            fprintf(file, "%5d (", results[i].params_tested[j]); // Write the pi value for the program
            const char* message = get_status_message(results[i].status[j]);
            fprintf(file, "%9s) ", message); // Write each status
        }
        fprintf(file, "\n");
    }

    fclose(file);
}


// TODO: Implement this function
double get_score(char *results_file, char *executable_name) {
    FILE* file = fopen(results_file, "r"); // Open the results file
    if (file == NULL) {
        perror("error opening results file");
        exit(1);
    }

    executable_name = get_exe_name(executable_name); // Get the executable name (minus the path)

    char line[BUFSIZ];

    // Read the first line to determine its length
    if (fgets(line, sizeof(line), file) == NULL) {
        perror("error reading first line");
        fclose(file);
        exit(1);
    }

    int line_length = strlen(line); // Length of the first line
    int line_number = atoi(executable_name + 4) - 1; // Assuming the format is "sol_X"

    fseek(file, line_number * line_length, SEEK_SET); // Seek to the line containing the executable's results

    if (fgets(line, sizeof(line), file) == NULL) { // Read the line containing the executable's results
        perror("error reading line");
        fclose(file);
        exit(1);
    }

    int correct_answers = 0;
    int total_tests = 0;
    char *p = strtok(line, " "); // Tokenize the line
    while (p != NULL) {
        if (strstr(p, "(") != NULL) { // Check if the token contains a "(" indicating a test
            total_tests++;
        } else if (strstr(p, "correct)") != NULL) { // Check if the test was correct
            correct_answers++;
        }
        p = strtok(NULL, " "); // Get the next token
    }

    fclose(file);
    return (double) correct_answers / total_tests;
}

void write_scores_to_file(autograder_results_t *results, int num_executables, char *results_file) {
    for (int i = 0; i < num_executables; i++) {
        double student_score = get_score(results_file, results[i].exe_path);
        char *student_exe = get_exe_name(results[i].exe_path);

        char score_file[] = "scores.txt";

        FILE *score_fp;
        if (i == 0)
            score_fp = fopen(score_file, "w");
        else
            score_fp = fopen(score_file, "a");

        if (!score_fp) {
            perror("Failed to open score file");
            exit(1);
        }

        int longest_len = get_longest_len_executable(results, num_executables);

        char format[20];
        sprintf(format, "%%-%ds: ", longest_len);
        fprintf(score_fp, format, student_exe);
        fprintf(score_fp, "%5.3f\n", student_score);

        fclose(score_fp);
    }
}
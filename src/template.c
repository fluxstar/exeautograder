#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>



void infinite_loop() {
    while(1){};    // Simulating a infinite loop
}

int main(int argc, char *argv[]) {
    #ifndef REDIR
        if (argc < 2) {
            // Usage for   EXEC:  argv[0] <param>    # Input is just param
            // Usage for   PIPE:  argv[0] <pipefd>   # Input is read end pipe fd
            // Usage for  REDIR:  argv[0]            # No param needed, read from stdin
            // Usage for MQUEUE:  argv[0] <param>    # Input is just param
            printf("Usage: %s <parameter | pipefd>\n", argv[0]);
            return 1;
        }
    #endif

    unsigned int seed = 0;

    for (int i = 0; argv[0][i] != '\0'; i++) {
        seed += (unsigned char)argv[0][i]; 
    }

    int param = 0;

    // TODO: Get input param from the different sources
    #ifdef EXEC
        
    
    param = atoi((char *) argv[1]);

    #elif REDIR
    
    scanf("%d", &param);
    
    #elif PIPE
    int fd = atoi(argv[1]);  // Convert the string to an integer

    write(STDERR_FILENO, &fd, sizeof(fd));
    fprintf(stderr, "%d", fd);

    if ((read(fd, &param, sizeof(param))) == -1) {
        perror("pipe read");
        exit(1);
    }
    
    close(fd);

    #elif MQUEUE


    #endif

    seed += param;
    srandom(seed);
  
    int mode = random() % 5 + 1;
    pid_t pid = getpid(); 

    switch (mode) {
        case 1:
            // Using fprintf(stderr, ...) since STDOUT is redirected to a file
            fprintf(stderr, "Program: %s, PID: %d, Mode: 1 - Exiting with status 0 (Correct answer)\n", argv[0], pid);
            // TODO: Write the result (0) to the output file (output/<executable>.<input>)
            //       Do not open the file. Think about what function you can use to output
            //       information given what you redirected in the autograder.c file.

            printf("%d", 1);
            break;
        case 2:
            fprintf(stderr, "Program: %s, PID: %d, Mode: 2 - Exiting with status 1 (Incorrect answer)\n", argv[0], pid);
            // TODO: Write the result (1) to the output file (same as case 1 above)
            
            printf("%d", 2);
            break;
        case 3:
            fprintf(stderr, "Program: %s, PID: %d, Mode: 3 - Triggering a segmentation fault\n", argv[0], pid);
            raise(SIGSEGV);  // Trigger a segmentation fault
            break;
        case 4:
            fprintf(stderr, "Program: %s, PID: %d, Mode: 4 - Entering an infinite loop\n", argv[0], pid);
            infinite_loop();
            break;
        case 5:
            fprintf(stderr, "Program: %s, PID: %d, Mode: 5 - Simulating being stuck/blocked\n", argv[0], pid);
            pause();  // Simulate being stuck/blocked
            break;
        default:
            break;
    }

    return 0;
}
Created by Group 31:
- abuka032 (Abdulwahid Abukar)
    -  Assisted with STDOUT/STDIN redirection
    -  Implemented transfer of input parameter through pipes
    - Implemented get_batch_size
    - Implemented remove_input_files
    - Implemented cancel_timer
    - Im
- krant115 (Logan Krant)
    - Implemented STDOUT/STDIN redirection skeleton
    - Wrote code to update results struct with statuses using output files
    - Used GDB for extensive debugging throughout the project
    - Implemented get_score function
    - Implemented remove_output_files function
    - Assisted with timer implementation
    - Assisted with mqueue debugging
- phimp003 (Jimmy Phimpadabsee)

How to compile the program: run "make <exec/redir/pipe/mqueue> N=<# of tests>" in the terminal while inside the p2 folder
Run the compiled autograder as follows: 
<exec/redir/pipe>: ./autograder solutions <p1>, <p2>, ...
<mqueue>: ./mq_autograder solutions <p1>, <p2>, ...

Assumptions:
- Each submission executable accepts a single integer parameter and returns (prints) 1 or 2 if it is correct or incorrect respectively.
- The results vary machine to machine, in our case, we used a CSE Lab Machine that supports Linux. (Important for get_batch_size function specifically)
- Solution executables match "sol_#" naming convention

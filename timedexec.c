#include <stdio.h>         // for printf(), perror(), etc.
#include <stdlib.h>        // for exit(), atoi()
#include <unistd.h>        // for fork(), execvp()
#include <sys/wait.h>      // for wait4()
#include <sys/resource.h>  // for getrusage() to measure CPU and memory
#include <sys/time.h>      // for setitimer() to implement timer
#include <signal.h>        // for setting signal handlers
#include <string.h>        // for string operations like strcmp
#include <getopt.h>        // for parsing --time and --help options
#include <errno.h>         // for checking system errors
#include <time.h>          // for wall-clock time tracking

// On macOS, ru_maxrss is in bytes; on Linux, it's in kilobytes.
#if defined(__APPLE__)
    #define MEMORY_UNIT_DIVISOR (1024.0 * 1024.0)  // convert bytes to MB
#else
    #define MEMORY_UNIT_DIVISOR 1024.0             // convert KB to MB
#endif

// Struct to hold child process info and time limit
typedef struct {
    pid_t pid;             // process ID of child
    int time_limit;        // time limit in seconds
} ChildData;

// Signal handler for SIGALRM (timer expiration)
void handle_alarm(int sig, siginfo_t *info, void *ucontext) {
    // Get the custom data we passed (child pid and time limit)
    ChildData *data = (ChildData *)info->si_value.sival_ptr;
    if (data->pid > 0) {
        // Print message and kill the child process if still running
        printf("\n[!] Time limit (%ds) exceeded. Terminating...\n", data->time_limit);
        kill(data->pid, SIGKILL);
    }
}

int main(int argc, char *argv[]) {
    // Store child process info and start wall clock timer
    ChildData child_data = { .pid = -1, .time_limit = 0 };
    time_t start_time = time(NULL);  // capture current wall clock time

    // Define long command-line options: --time and --help
    struct option long_options[] = {
        {"time", required_argument, 0, 't'},
        {"help", no_argument,       0, 'h'},
        {0, 0, 0, 0} // marks end of array
    };

    // Parse command-line arguments
    int opt;
    while ((opt = getopt_long(argc, argv, "t:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 't':
                // Convert time argument to integer
                child_data.time_limit = atoi(optarg);
                if (child_data.time_limit <= 0) {
                    fprintf(stderr, "Error: Time limit must be positive\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'h':
                // Print usage/help message and exit
                printf("Timedexec - Run commands with time limits\n\n");
                printf("Usage: %s --time SECONDS COMMAND [ARGS...]\n\n", argv[0]);
                printf("Examples:\n");
                printf("  %s --time 5 sleep 10   # Kills after 5 seconds\n", argv[0]);
                printf("  %s --time 1 ls -l      # Lists files (max 1 second)\n", argv[0]);
                printf("  %s --time 0.5 ./a.out  # Sub-second precision\n", argv[0]);
                exit(EXIT_SUCCESS);
            default:
                // Unrecognized option
                fprintf(stderr, "Try '%s --help' for usage\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // Ensure user provided a command to run after options
    if (optind >= argc) {
        fprintf(stderr, "Error: No command specified\n");
        fprintf(stderr, "Usage: %s --time SECONDS COMMAND [ARGS...]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Setup signal handler for SIGALRM with custom handler
    struct sigaction sa = {
        .sa_sigaction = handle_alarm,         // handler function
        .sa_flags = SA_SIGINFO | SA_RESTART  // get siginfo + auto-restart syscalls
    };
    sigemptyset(&sa.sa_mask);                // no signals blocked during handler
    sigaction(SIGALRM, &sa, NULL);           // register the handler

    // Fork the process to run the command in a child
    if ((child_data.pid = fork()) == -1) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }

    if (child_data.pid == 0) {  // Child process
        // If on Linux, ensure child dies if parent crashes
        #ifdef __linux__
            prctl(PR_SET_PDEATHSIG, SIGKILL);
        #endif

        // Replace child with user's command
        execvp(argv[optind], &argv[optind]);

        // If exec fails, print error
        fprintf(stderr, "Failed to execute '%s': ", argv[optind]);
        perror(NULL);
        exit(EXIT_FAILURE);
    } 
    else {  // Parent process
        // Set timer to kill child if it runs too long
        if (child_data.time_limit > 0) {
            struct itimerval timer = {
                .it_value = { 
                    .tv_sec = child_data.time_limit,
                    .tv_usec = 0 
                }
            };
            setitimer(ITIMER_REAL, &timer, NULL);  // starts countdown
        }

        // Wait for child to finish, and collect resource usage
        int status;
        struct rusage usage;
        if (wait4(child_data.pid, &status, 0, &usage) == -1) {
            perror("wait4 failed");
            exit(EXIT_FAILURE);
        }

        // Print summary of execution
        printf("\n[+] Execution Complete\n");
        printf("Wall-clock time:    %ld seconds\n", time(NULL) - start_time);
        printf("User CPU time:      %ld.%06d seconds\n", 
               (long)usage.ru_utime.tv_sec, (int)usage.ru_utime.tv_usec);
        printf("System CPU time:    %ld.%06d seconds\n", 
               (long)usage.ru_stime.tv_sec, (int)usage.ru_stime.tv_usec);
        printf("Max memory used:    %.2f MB\n", 
               (double)usage.ru_maxrss / MEMORY_UNIT_DIVISOR);

        // Report exit cause
        if (WIFEXITED(status)) {
            printf("Exit status:        %d\n", WEXITSTATUS(status));
        } 
        else if (WIFSIGNALED(status)) {
            printf("Terminated by:      signal %d", WTERMSIG(status));
            if (WTERMSIG(status) == SIGKILL) {
                printf(" (SIGKILL: Timeout enforced)");
            }
            printf("\n");
        }
    }

    return 0;
}
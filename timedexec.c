#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <time.h>

// Platform-specific adaptations
#if defined(__APPLE__)
    #define MEMORY_UNIT_DIVISOR (1024.0 * 1024.0)  // bytes → MB
#else
    #define MEMORY_UNIT_DIVISOR 1024.0  // KB → MB
#endif

typedef struct {
    pid_t pid;
    int time_limit;
} ChildData;

// Signal handler with context
void handle_alarm(int sig, siginfo_t *info, void *ucontext) {
    ChildData *data = (ChildData *)info->si_value.sival_ptr;
    if (data->pid > 0) {
        printf("\n[!] Time limit (%ds) exceeded. Terminating...\n", data->time_limit);
        kill(data->pid, SIGKILL);
    }
}

int main(int argc, char *argv[]) {
    // Configuration
    ChildData child_data = { .pid = -1, .time_limit = 0 };
    time_t start_time = time(NULL);

    // Command-line options
    struct option long_options[] = {
        {"time", required_argument, 0, 't'},
        {"help", no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    // Parse arguments
    int opt;
    while ((opt = getopt_long(argc, argv, "t:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 't':
                child_data.time_limit = atoi(optarg);
                if (child_data.time_limit <= 0) {
                    fprintf(stderr, "Error: Time limit must be positive\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'h':
   		 printf("Timedexec - Run commands with time limits\n\n");
    		 printf("Usage: %s --time SECONDS COMMAND [ARGS...]\n\n", argv[0]);
   		 printf("Examples:\n");
  		 printf("  %s --time 5 sleep 10   # Kills after 5 seconds\n", argv[0]);
   		 printf("  %s --time 1 ls -l      # Lists files (max 1 second)\n", argv[0]);
    		 printf("  %s --time 0.5 ./a.out  # Sub-second precision\n", argv[0]);
    		 exit(EXIT_SUCCESS);            default:
               	 fprintf(stderr, "Try '%s --help' for usage\n", argv[0]);
               	 exit(EXIT_FAILURE);
        }
    }

    // Validate command
    if (optind >= argc) {
        fprintf(stderr, "Error: No command specified\n");
        fprintf(stderr, "Usage: %s --time SECONDS COMMAND [ARGS...]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Configure signal handling
    struct sigaction sa = {
        .sa_sigaction = handle_alarm,
        .sa_flags = SA_SIGINFO | SA_RESTART
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);

    // Fork process
    if ((child_data.pid = fork()) == -1) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }

    if (child_data.pid == 0) {  // Child
        // Linux-only: Ensure child dies if parent crashes
        #ifdef __linux__
            prctl(PR_SET_PDEATHSIG, SIGKILL);
        #endif

        execvp(argv[optind], &argv[optind]);
        fprintf(stderr, "Failed to execute '%s': ", argv[optind]);
        perror(NULL);
        exit(EXIT_FAILURE);
    } 
    else {  // Parent
        // Set timer (higher precision than alarm())
        if (child_data.time_limit > 0) {
            struct itimerval timer = {
                .it_value = { 
                    .tv_sec = child_data.time_limit,
                    .tv_usec = 0 
                }
            };
            setitimer(ITIMER_REAL, &timer, NULL);
        }

        // Wait for child and capture stats
        int status;
        struct rusage usage;
        if (wait4(child_data.pid, &status, 0, &usage) == -1) {
            perror("wait4 failed");
            exit(EXIT_FAILURE);
        }

        // Report results
        printf("\n[+] Execution Complete\n");
        printf("Wall-clock time:    %ld seconds\n", time(NULL) - start_time);
        printf("User CPU time:      %ld.%06d seconds\n", 
               (long)usage.ru_utime.tv_sec, (int)usage.ru_utime.tv_usec);
        printf("System CPU time:    %ld.%06d seconds\n", 
               (long)usage.ru_stime.tv_sec, (int)usage.ru_stime.tv_usec);
        printf("Max memory used:    %.2f MB\n", 
               (double)usage.ru_maxrss / MEMORY_UNIT_DIVISOR);

        // Exit analysis
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
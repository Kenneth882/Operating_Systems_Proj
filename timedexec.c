#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <signal.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <time.h>

pid_t child_pid = -1;

void handle_alarm(int sig) {
    if (child_pid > 0) {
        printf("\n[!] Time limit exceeded. Terminating process...\n");
        kill(child_pid, SIGKILL);
    }
}

int main(int argc, char *argv[]) {
    int    time_limit  = 0;
    time_t start_time  = time(NULL);

    struct option long_options[] = {
        {"time", required_argument, 0, 't'},
        {"help", no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "t:h", long_options, NULL)) != -1) {
        switch (opt) {
        case 't':
            time_limit = atoi(optarg);
            break;
        case 'h':
            printf("Usage: %s --time SECONDS COMMAND [ARGS...]\n", argv[0]);
            exit(EXIT_SUCCESS);
        default:
            fprintf(stderr, "Try '%s --help' for usage\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Error: No command specified\n");
        exit(EXIT_FAILURE);
    }

    signal(SIGALRM, handle_alarm);

    child_pid = fork();
    if (child_pid == -1) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }

    if (child_pid == 0) {
        execvp(argv[optind], &argv[optind]);
        perror("execvp failed");
        exit(EXIT_FAILURE);
    } else {
        if (time_limit > 0)
            alarm(time_limit);

        int status;
        struct rusage usage;

        if (wait4(child_pid, &status, 0, &usage) == -1) {
            perror("wait4 failed");
            exit(EXIT_FAILURE);
        }

        printf("\n[+] Execution Complete\n");
        printf("Wall-clock time: %ld seconds\n", time(NULL) - start_time);
        printf("User  CPU time:  %ld.%06d seconds\n",
               (long)usage.ru_utime.tv_sec,
               (int) usage.ru_utime.tv_usec);
        printf("System CPU time: %ld.%06d seconds\n",
               (long)usage.ru_stime.tv_sec,
               (int) usage.ru_stime.tv_usec);
        printf("Max memory used: %.2f MB\n",
               (double)usage.ru_maxrss / 1024.0);

        if (WIFEXITED(status))
            printf("Exit status: %d\n", WEXITSTATUS(status));
        else if (WIFSIGNALED(status))
            printf("Terminated by signal: %d\n", WTERMSIG(status));
    }
    return 0;
}

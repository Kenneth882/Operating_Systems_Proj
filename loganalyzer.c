#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <ctype.h>

typedef struct {
    size_t total, info, warn, error, debug, trace, other;
} stats_t;

typedef struct {
    const char *start;
    const char *end; 
    int min_level;
    stats_t local;
} thread_arg_t;

static volatile sig_atomic_t stop_now = 0;
static pthread_mutex_t aggregate_mutex = PTHREAD_MUTEX_INITIALIZER;
static stats_t aggregate = {0};

static void on_signal(int signo) {
    (void)signo;
    stop_now = 1;
}


enum { LVL_TRACE, LVL_DEBUG, LVL_INFO, LVL_WARN, LVL_ERROR, LVL_UNKNOWN };

static int str_to_level(const char *s) {
    if (!s) return LVL_UNKNOWN;
    if (!strcasecmp(s, "trace")) return LVL_TRACE;
    if (!strcasecmp(s, "debug")) return LVL_DEBUG;
    if (!strcasecmp(s, "info" )) return LVL_INFO;
    if (!strcasecmp(s, "warn" )) return LVL_WARN;
    if (!strcasecmp(s, "error")) return LVL_ERROR;
    return LVL_UNKNOWN;
}

static const char *level_to_str(int lvl) {
    switch (lvl) {
        case LVL_TRACE: return "TRACE"; case LVL_DEBUG: return "DEBUG";
        case LVL_INFO : return "INFO" ; case LVL_WARN : return "WARN" ;
        case LVL_ERROR: return "ERROR"; default        : return "UNKNOWN"; }
}

static void add_stats(stats_t *a, const stats_t *b) {
    a->total += b->total; a->info  += b->info;  a->warn  += b->warn;
    a->error += b->error; a->debug += b->debug; a->trace += b->trace;
    a->other += b->other;
}

static void *analyze_chunk(void *arg) {
    thread_arg_t *ta = (thread_arg_t *)arg;
    const char *p = ta->start;

    while (p < ta->end && !stop_now) {
        const char *line_start = p;
        while (p < ta->end && *p != '\n') ++p;
        const char *line_end = p;
        if (p < ta->end) ++p; 


        int lvl = LVL_UNKNOWN;
        const char *lb = memchr(line_start, '[', line_end - line_start);
        if (lb) {
            const char *rb = memchr(lb, ']', line_end - lb);
            if (rb && rb - lb <= 8) {
                char tmp[8] = {0};
                size_t len = rb - lb - 1;
                if (len < sizeof(tmp)) {
                    memcpy(tmp, lb + 1, len);
                    lvl = str_to_level(tmp);
                }
            }
        }

        
        if (lvl < ta->min_level) continue;

        
        ta->local.total++;
        switch (lvl) {
            case LVL_INFO : ta->local.info++;  break;
            case LVL_WARN : ta->local.warn++;  break;
            case LVL_ERROR: ta->local.error++; break;
            case LVL_DEBUG: ta->local.debug++; break;
            case LVL_TRACE: ta->local.trace++; break;
            default:        ta->local.other++; break;
        }
    }

    
    pthread_mutex_lock(&aggregate_mutex);
    add_stats(&aggregate, &ta->local);
    pthread_mutex_unlock(&aggregate_mutex);

    return NULL;
}

static void usage(const char *prog) {
    fprintf(stderr,
"Usage: %s -f <logfile> [OPTIONS]\n\n"
"Options:\n"
"  -f, --file FILE       Path to log file (required)\n"
"  -t, --threads N       Number of worker threads (default: 1)\n"
"  -l, --level LEVEL     Minimum severity to count (INFO, WARN, ERROR, ...)\n"
"  -h, --help            Show this help and exit\n", prog);
}

int main(int argc, char *argv[]) {
    const char *file_path = NULL;
    int threads = 1;
    int min_level = LVL_TRACE;  

    static struct option long_opts[] = {
        {"file",    required_argument, 0, 'f'},
        {"threads", required_argument, 0, 't'},
        {"level",   required_argument, 0, 'l'},
        {"help",    no_argument,       0, 'h'},
        {0,0,0,0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "f:t:l:h", long_opts, NULL)) != -1) {
        switch (c) {
            case 'f': file_path = optarg; break;
            case 't': threads = atoi(optarg); if (threads < 1) threads = 1; break;
            case 'l': min_level = str_to_level(optarg); break;
            case 'h': usage(argv[0]); return EXIT_SUCCESS;
            default : usage(argv[0]); return EXIT_FAILURE;
        }
    }

    if (!file_path) { usage(argv[0]); return EXIT_FAILURE; }

    struct sigaction sa = { .sa_handler = on_signal };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    int fd = open(file_path, O_RDONLY);
    if (fd == -1) { perror("open"); return EXIT_FAILURE; }

    struct stat st;
    if (fstat(fd, &st) == -1) { perror("fstat"); return EXIT_FAILURE; }
    if (!S_ISREG(st.st_mode) || st.st_size == 0) {
        fprintf(stderr, "Invalid file or empty: %s\n", file_path);
        return EXIT_FAILURE;
    }

    char *data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) { perror("mmap"); return EXIT_FAILURE; }

    if (threads > st.st_size / (1 << 20)) threads = st.st_size / (1 << 20) ?: 1; 

    pthread_t *tids = calloc(threads, sizeof(*tids));
    thread_arg_t *args = calloc(threads, sizeof(*args));
    if (!tids || !args) { perror("calloc"); return EXIT_FAILURE; }

    size_t chunk = st.st_size / threads;
    for (int i = 0; i < threads; ++i) {
        const char *start = data + i * chunk;
        const char *end   = (i == threads -1) ? data + st.st_size
                                             : data + (i + 1) * chunk;
        if (i != 0) while (start < end && *start != '\n') ++start;

        args[i].start = start;
        args[i].end = end;
        args[i].min_level = min_level;
        memset(&args[i].local, 0, sizeof(stats_t));

        if (pthread_create(&tids[i], NULL, analyze_chunk, &args[i]) != 0) {
            perror("pthread_create"); return EXIT_FAILURE; }
    }

    for (int i = 0; i < threads; ++i) pthread_join(tids[i], NULL);

    munmap(data, st.st_size);
    close(fd);


    printf("\n===== loganalyzer SUMMARY =====\n");
    printf("Total lines analyzed : %zu\n", aggregate.total);
    printf("  INFO  : %zu\n", aggregate.info);
    printf("  WARN  : %zu\n", aggregate.warn);
    printf("  ERROR : %zu\n", aggregate.error);
    printf("  DEBUG : %zu\n", aggregate.debug);
    printf("  TRACE : %zu\n", aggregate.trace);
    printf("  OTHER : %zu\n", aggregate.other);

    return EXIT_SUCCESS;
}

//    ./loganalyzer -f demo.log
//      ./loganalyzer -f demo.log -t 4
//      ./loganalyzer -f demo.log -l error
//     
//      time ./loganalyzer -f big.log -t 8
//      time ./loganalyzer -f big.log -t 128
//     ./loganalyzer -f big.log -t 8



//     TEST FILE

//Test_file
//   ./loganalyzer_test.sh
// ./timedexec --time 10 -- ./loganalyzer -f big.log -t 4







#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <dirent.h>
#include <ctype.h>

#define BUFFER_SIZE 4096

typedef struct {
    unsigned long start;
    unsigned long end;
    unsigned long size;
    char permissions[5];
    unsigned long offset;
    char device[8];
    unsigned long inode;
    char pathname[PATH_MAX];
} MemoryRegion;

// Function to display help
void display_help(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("Display memory usage information for processes or system.\n\n");
    printf("Options:\n");
    printf("  -p PID      Display memory maps for the specified process ID\n");
    printf("  -s          Display system memory information\n");
    printf("  -m          Display shared memory segments\n");
    printf("  -f FILTER   Filter memory regions by type (heap, stack, anon, file, etc.)\n");
    printf("  -v          Verbose output with more details\n");
    printf("  -h          Display this help and exit\n");
}

// Parse memory maps line into a structured format
void parse_memory_line(char *line, MemoryRegion *region) {
    char perms[5] = {0};
    char device[8] = {0};
    char pathname[PATH_MAX] = {0};
    
    memset(region, 0, sizeof(MemoryRegion));
    
    // Format of /proc/PID/maps line:
    // address           perms offset  dev   inode   pathname
    // 08048000-08056000 r-xp 00000000 03:0c 64593   /usr/sbin/gpm
    
    sscanf(line, "%lx-%lx %4s %lx %7s %lu %s",
           &region->start, &region->end, 
           perms, &region->offset, 
           device, &region->inode, 
           pathname);
    
    strncpy(region->permissions, perms, 4);
    strncpy(region->device, device, 7);
    strncpy(region->pathname, pathname, PATH_MAX - 1);
    
    region->size = region->end - region->start;
}

// Function to determine memory region type
const char* get_region_type(MemoryRegion *region) {
    if (strstr(region->pathname, "[heap]"))
        return "heap";
    else if (strstr(region->pathname, "[stack]"))
        return "stack";
    else if (strstr(region->pathname, "[vdso]"))
        return "vdso";
    else if (strstr(region->pathname, "[vsyscall]"))
        return "vsyscall";
    else if (region->pathname[0] == '\0')
        return "anonymous";
    else if (strstr(region->pathname, "SYSV"))
        return "shared_memory";
    else if (region->pathname[0] == '/')
        return "file_mapped";
    else
        return "other";
}

// Calculate memory usage summary
void calculate_memory_summary(MemoryRegion *regions, int count, int verbose) {
    unsigned long total_size = 0;
    unsigned long heap_size = 0;
    unsigned long stack_size = 0;
    unsigned long shared_size = 0;
    unsigned long lib_size = 0;
    unsigned long anon_size = 0;
    
    for (int i = 0; i < count; i++) {
        total_size += regions[i].size;
        
        const char *type = get_region_type(&regions[i]);
        if (strcmp(type, "heap") == 0)
            heap_size += regions[i].size;
        else if (strcmp(type, "stack") == 0)
            stack_size += regions[i].size;
        else if (strcmp(type, "shared_memory") == 0 || (regions[i].permissions[0] == 'r' && regions[i].permissions[3] == 's'))
            shared_size += regions[i].size;
        else if (strcmp(type, "file_mapped") == 0 && strstr(regions[i].pathname, ".so"))
            lib_size += regions[i].size;
        else if (strcmp(type, "anonymous") == 0)
            anon_size += regions[i].size;
    }
    
    printf("\nMemory Usage Summary:\n");
    printf("-----------------------------------------------------\n");
    printf("Total memory used:       %10lu KB (%lu bytes)\n", total_size / 1024, total_size);
    printf("Heap memory:             %10lu KB (%lu bytes)\n", heap_size / 1024, heap_size);
    printf("Stack memory:            %10lu KB (%lu bytes)\n", stack_size / 1024, stack_size);
    printf("Shared memory:           %10lu KB (%lu bytes)\n", shared_size / 1024, shared_size);
    printf("Library memory:          %10lu KB (%lu bytes)\n", lib_size / 1024, lib_size);
    printf("Anonymous memory:        %10lu KB (%lu bytes)\n", anon_size / 1024, anon_size);
}

// Show detailed process memory information
void show_process_memory(pid_t pid, const char *filter, int verbose) {
    char path[PATH_MAX];
    char buffer[BUFFER_SIZE];
    MemoryRegion regions[1000]; // Assuming max 1000 regions, adjust as needed
    int region_count = 0;
    
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    
    FILE *file = fopen(path, "r");
    if (!file) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    
    printf("Memory maps for PID %d:\n", pid);
    printf("-----------------------------------------------------\n");
    
    if (verbose) {
        printf("%-16s %-8s %-16s %-7s %-8s %s\n", 
               "Address Range", "Perms", "Size", "Offset", "Inode", "Pathname");
        printf("-----------------------------------------------------\n");
    }
    
    // Read and process each line
    while (fgets(buffer, sizeof(buffer), file) && region_count < 1000) {
        buffer[strcspn(buffer, "\n")] = 0; // Remove newline
        
        parse_memory_line(buffer, &regions[region_count]);
        const char *type = get_region_type(&regions[region_count]);
        
        // Apply filter if specified
        if (filter && strcmp(filter, type) != 0) {
            continue;
        }
        
        if (verbose) {
            printf("%016lx-%016lx %s %8lu KB %8lx %8lu %s [%s]\n", 
                   regions[region_count].start, regions[region_count].end,
                   regions[region_count].permissions,
                   regions[region_count].size / 1024,
                   regions[region_count].offset,
                   regions[region_count].inode,
                   regions[region_count].pathname,
                   type);
        } else {
            printf("%016lx-%016lx %s %8lu KB %s\n", 
                   regions[region_count].start, regions[region_count].end,
                   regions[region_count].permissions,
                   regions[region_count].size / 1024,
                   regions[region_count].pathname);
        }
        
        region_count++;
    }
    
    fclose(file);
    
    // Print memory usage summary
    calculate_memory_summary(regions, region_count, verbose);
    
    // Additional process info
    if (verbose) {
        // Show status information
        snprintf(path, sizeof(path), "/proc/%d/status", pid);
        file = fopen(path, "r");
        if (file) {
            printf("\nProcess Status Information:\n");
            printf("-----------------------------------------------------\n");
            while (fgets(buffer, sizeof(buffer), file)) {
                if (strncmp(buffer, "Vm", 2) == 0) { // Show only memory-related status
                    printf("%s", buffer);
                }
            }
            fclose(file);
        }
    }
}

// Show system-wide shared memory segments
void show_shared_memory() {
    struct shmid_ds shm_info;
    int id;
    
    printf("System Shared Memory Segments:\n");
    printf("-----------------------------------------------------\n");
    printf("%-10s %-10s %-10s %-10s %-10s %s\n",
           "ID", "Key", "Size (KB)", "Owner", "Perms", "Attached");
    
    // Loop through possible shared memory IDs
    for (id = 0; id < 100; id++) {
        if (shmctl(id, IPC_STAT, &shm_info) != -1) {
            printf("%-10d %-10d %-10lu %-10d %-10o %lu\n",
                   id,
                   (int)shm_info.shm_perm.__key,
                   (unsigned long)(shm_info.shm_segsz / 1024),
                   shm_info.shm_perm.uid,
                   shm_info.shm_perm.mode & 0777,
                   shm_info.shm_nattch);
        }
    }
}

// Show system memory information
void show_system_memory(int verbose) {
    FILE *file = fopen("/proc/meminfo", "r");
    if (!file) {
        perror("fopen /proc/meminfo");
        exit(EXIT_FAILURE);
    }
    
    printf("System Memory Information:\n");
    printf("-----------------------------------------------------\n");
    
    char buffer[BUFFER_SIZE];
    unsigned long total_mem = 0;
    unsigned long free_mem = 0;
    unsigned long available_mem = 0;
    unsigned long cached_mem = 0;
    
    while (fgets(buffer, sizeof(buffer), file)) {
        printf("%s", buffer);
        
        if (strncmp(buffer, "MemTotal:", 9) == 0) {
            sscanf(buffer, "MemTotal: %lu", &total_mem);
        } else if (strncmp(buffer, "MemFree:", 8) == 0) {
            sscanf(buffer, "MemFree: %lu", &free_mem);
        } else if (strncmp(buffer, "MemAvailable:", 13) == 0) {
            sscanf(buffer, "MemAvailable: %lu", &available_mem);
        } else if (strncmp(buffer, "Cached:", 7) == 0) {
            sscanf(buffer, "Cached: %lu", &cached_mem);
        }
    }
    
    fclose(file);
    
    // Print memory usage summary
    printf("\nMemory Usage Summary:\n");
    printf("-----------------------------------------------------\n");
    if (total_mem > 0) {
        float used_percent = 100.0 * (total_mem - free_mem) / total_mem;
        float avail_percent = 100.0 * available_mem / total_mem;
        
        printf("Total Memory:     %10lu KB\n", total_mem);
        printf("Used Memory:      %10lu KB (%.1f%%)\n", total_mem - free_mem, used_percent);
        printf("Free Memory:      %10lu KB\n", free_mem);
        printf("Available Memory: %10lu KB (%.1f%%)\n", available_mem, avail_percent);
        printf("Cached Memory:    %10lu KB\n", cached_mem);
    }
    
    if (verbose) {
        // Show additional memory-related info from /proc
        printf("\nMemory Distribution by Process:\n");
        printf("-----------------------------------------------------\n");
        printf("%-8s %-20s %-12s\n", "PID", "Process", "Memory (KB)");
        
        DIR *proc_dir = opendir("/proc");
        if (proc_dir) {
            struct dirent *entry;
            
            while ((entry = readdir(proc_dir))) {
                // Check if the entry is a process directory (all digits)
                int is_pid = 1;
                for (int i = 0; entry->d_name[i]; i++) {
                    if (!isdigit(entry->d_name[i])) {
                        is_pid = 0;
                        break;
                    }
                }
                
                if (is_pid) {
                    char stat_path[PATH_MAX];
                    char cmd_path[PATH_MAX];
                    char cmd[256] = "<unknown>";
                    int proc_pid = atoi(entry->d_name);
                    unsigned long vm_size = 0;
                    
                    // Get process name
                    snprintf(cmd_path, sizeof(cmd_path), "/proc/%s/comm", entry->d_name);
                    FILE *cmd_file = fopen(cmd_path, "r");
                    if (cmd_file) {
                        if (fgets(cmd, sizeof(cmd), cmd_file)) {
                            cmd[strcspn(cmd, "\n")] = 0; // Remove newline
                        }
                        fclose(cmd_file);
                    }
                    
                    // Get memory usage
                    snprintf(stat_path, sizeof(stat_path), "/proc/%s/status", entry->d_name);
                    FILE *stat_file = fopen(stat_path, "r");
                    if (stat_file) {
                        char line[256];
                        while (fgets(line, sizeof(line), stat_file)) {
                            if (strncmp(line, "VmRSS:", 6) == 0) {
                                sscanf(line, "VmRSS: %lu", &vm_size);
                                break;
                            }
                        }
                        fclose(stat_file);
                        
                        printf("%-8d %-20s %-12lu\n", proc_pid, cmd, vm_size);
                    }
                }
            }
            closedir(proc_dir);
        }
    }
}

int main(int argc, char *argv[]) {
    pid_t pid = -1;
    int system_flag = 0;
    int shared_flag = 0;
    int verbose_flag = 0;
    char *filter = NULL;
    int opt;
    
    if (argc == 1) {
        display_help(argv[0]);
        exit(EXIT_SUCCESS);
    }
    
    while ((opt = getopt(argc, argv, "p:smf:vh")) != -1) {
        switch (opt) {
            case 'p':
                pid = atoi(optarg);
                break;
            case 's':
                system_flag = 1;
                break;
            case 'm':
                shared_flag = 1;
                break;
            case 'f':
                filter = optarg;
                break;
            case 'v':
                verbose_flag = 1;
                break;
            case 'h':
                display_help(argv[0]);
                exit(EXIT_SUCCESS);
                break;
            default:
                fprintf(stderr, "Try '%s -h' for more information.\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    
    if (pid != -1) {
        show_process_memory(pid, filter, verbose_flag);
    } else if (system_flag) {
        show_system_memory(verbose_flag);
    } else if (shared_flag) {
        show_shared_memory();
    } else {
        fprintf(stderr, "Please specify -p <pid>, -s for system memory, or -m for shared memory.\n");
        exit(EXIT_FAILURE);
    }
    
    return 0;
}

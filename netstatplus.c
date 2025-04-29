#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <getopt.h>
#include <poll.h>

// ********************* OPTION VARIABLES ***********************
int once = 0;
int tcp = 0;
int udp = 0;
int listening = 0;
int all = 0;
int statistics = 0;
int interval = 1;

// **************************** CONNECTION STATES *******************
const char* states[] = {
    "UNKNOWN", "ESTABLISHED", "SYN_SENT", "SYN_RECV", "FIN_WAIT1",
    "FIN_WAIT2", "TIME_WAIT", "CLOSE", "CLOSE_WAIT", "LAST_ACK",
    "LISTEN", "CLOSING", "NEW_SYN_RECV"
};

// ******************************** STATISTIC MAPPINGS ****************************
struct StatMap {
    const char* field_name;
    const char* format_string;
};

static struct StatMap tcp_map[] = {
    { "ActiveOpens",    "active connection openings" },
    { "PassiveOpens",   "passive connection openings" },
    { "AttemptFails",   "failed connection attempts" },
    { "EstabResets",    "connection resets received" },
    { "CurrEstab",      "connections established" },
    { "InSegs",         "segments received" },
    { "OutSegs",        "segments sent out" },
    { "RetransSegs",    "segments retransmitted" },
    { "InErrs",         "bad segments received" },
    { "OutRsts",        "resets sent" },
    { NULL,             NULL }
};

static struct StatMap udp_map[] = {
    { "InDatagrams",    "UDP packets received" },
    { "NoPorts",        "packets to unknown port received" },
    { "InErrors",       "receive errors" },
    { "OutDatagrams",   "UDP packets sent" },
    { "RcvbufErrors",   "receive buffer errors" },
    { "SndbufErrors",   "send buffer errors" },
    { NULL,             NULL }
};

static struct StatMap ip_map[] = {
    { "InReceives",     "total packets received" },
    { "ForwDatagrams",  "forwarded" },
    { "InDiscards",     "incoming packets discarded" },
    { "InDelivers",     "incoming packets delivered" },
    { "OutRequests",    "requests sent out" },
    { NULL,             NULL }
};

static struct StatMap icmp_map[] = {
    { "InMsgs",         "ICMP messages received" },
    { "OutMsgs",        "ICMP messages sent" },
    { NULL,             NULL }
};

// ******************************** FUNCTION PROTOTYPES ********************************************
void display_connections(char* proto);
void hex_to_ip(char* hex_ip, char* ip_str, int ip_str_size);
void display_statistics(char* proto_label, struct StatMap proto_map[]);

// *********************************** MAIN ********************************************************
int main(int argc, char* argv[]) {
// get option provided by user
    int opt;
    while ((opt = getopt(argc, argv, "i:outals")) != -1) {
        switch (opt) {
            case 'o': // run once
                once = 1;
                break;
            case 'i': // refresh at intervals of i seconds
                interval = atoi(optarg);
                break;
            case 't': // show only tcp
                tcp = 1;
                break;
            case 'u': // show only udp
                udp = 1;
                break;
            case 'a': // show all connections, active or not
                all = 1;
                break;
            case 'l': // show listening connections only
                listening = 1;
                break;
            case 's': // show statistics
                statistics = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-i interval] [-a]  [-l] [-t] [-u] [-s] [-o]\n", argv[0]);
                printf("-i [interval]        refreshes netstatplus every i seconds\n");
                printf("-a                   displays all sockets (default: connected) \n");
                printf("-l                   display listening sockets\n");
                printf("-t                    display tcp only\n");
                printf("-u                    display udp only\n");
                printf("-s                    display networking statistics\n");
                printf("-o                    display netstatplus only once\n");
                return 1;
        }
    }
    // if -u or -t not given by user, enable both
    if (!tcp && !udp) {
        tcp = 1;
        udp = 1;
    }
// set up poll to wait for input
    struct pollfd fds[1];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
// continue until user quits
    while (1) {
    // clear screen after every refresh
        system("clear");
    // display statistics if enabled
        if (statistics == 1) {
            display_statistics("Ip:", ip_map);
            display_statistics("Icmp:", icmp_map);
            if (tcp) display_statistics("Tcp:", tcp_map);
            if (udp) display_statistics("Udp:", udp_map);
        }
    // display all connections if enabled
        if (all) {
            printf("Active Internet Connections (servers and established)\n");
            printf("%s %s %-10s %-25s %-25s %s\n", "Proto", "Recv-Q", "Send-Q", "Local Address", "Foreign Address", "State");
            if (tcp) display_connections("tcp");
            if (udp) display_connections("udp");
        } else if (listening) {
            printf("Active Internet Connections (servers only)\n");
            printf("%s %s %-10s %-25s %-25s %s\n", "Proto", "Recv-Q", "Send-Q", "Local Address", "Foreign Address", "State");
            if (tcp) display_connections("tcp");
            if (udp) display_connections("udp");
        } else {
            printf("Active Internet Connections (no servers)\n");
            printf("%s %s %-10s %-25s %-25s %s\n", "Proto", "Recv-Q", "Send-Q", "Local Address", "Foreign Address", "State");
            if (tcp) display_connections("tcp");
            if (udp) display_connections("udp");
        }
    // display connections only once if enabled
        if (once) break;
    // Options while running
        printf("#####################################################################\n");
        printf("Options while running:\n");
        printf("-1                    quit program\n");
        printf("-q                    refreshes display\n");
        printf("-a                    toggle all sockets\n");
        printf("-l                    toggle listening sockets\n");
        printf("-t                    toggle tcp only\n");
        printf("-u                    toggle udp only\n");
        printf("-s                    toggle networking statistics\n");
        printf("#####################################################################\n");
    // poll()
        int var = poll(fds, 1, interval * 1000);
        if (var == -1) {
            perror("Poll failed.");
            break;
        } 
    // accept user input entered while running
        else if (var > 0) {
            if (fds[0].revents && POLLIN) {
                char c;
                read(STDIN_FILENO, &c, 1);
                switch (c) {
                    case 'q': // quits program
                        printf("Quitting netstatplus.\n");
                        return 0;
                        break;
                    case 'r': // refreshes program
                        continue;
                    case 't': // toggles tcp
                        tcp = !tcp;
                        break;
                    case 'u': // toggles udp
                        udp = !udp;
                        break;
                    case 'a': // toggles all
                        all = !all;
                        break;
                    case 'l': // toggles listening
                        listening = !listening;
                        break;
                    case 's': // toggles statistics
                        statistics = !statistics;
                        break;
                }
            }
        }

    }

    return 0;
}

// ****************************************** FUNCTIONS *********************************************
void display_connections(char* proto) { // displays table of tcp and udp connections
// open file
    char filename[15];
    snprintf(filename, sizeof(filename), "/proc/net/%s", proto);
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open file.");
        exit(EXIT_FAILURE);
    }
// read file
    char buffer[2048];
    int readsize;
    readsize = read(fd, buffer, sizeof(buffer)-1);
    if (readsize <= 0) {
        perror("Failed to read file.");
        close(fd);
        exit(EXIT_FAILURE);
    }
// close file
    buffer[readsize] = '\0';
    close(fd);
// parse file contents
    char* line = strtok(buffer, "\n"); // skip header
    while ((line = strtok(NULL, "\n")) != NULL) {
    // connection table categories
        int local_port, remote_port, state;
        char local_ip_hex[9], remote_ip_hex[9];
        int tx, rx;
        unsigned int inode;
    // parse though line
        int parsed = sscanf(line, "%*s %8[0-9A-Fa-f]:%x %8[0-9A-Fa-f]:%x %x %x:%x %*s %*s %*s %*s %u",
        local_ip_hex, &local_port, remote_ip_hex, &remote_port, &state, &tx, &rx, &inode);
        if (parsed < 8) continue;
    // exclude certain lines based on options chosen by user
        if (listening == 1 && all == 0 && (strcmp(states[state], "LISTEN") != 0)) continue;
        if (listening == 0 && all == 0 && (strcmp(states[state], "LISTEN") == 0)) continue;
    // put togehter ip address and port
        char local_addr[25], remote_addr[25];
        char local_ip[16], remote_ip[16];
        hex_to_ip(local_ip_hex, local_ip, sizeof(local_ip));
        snprintf(local_addr, sizeof(local_addr), "%s:%d", local_ip, local_port);
        hex_to_ip(remote_ip_hex, remote_ip, sizeof(remote_ip));
        snprintf(remote_addr, sizeof(remote_addr), "%s:%d", remote_ip, remote_port);
    // print line
        printf("%-5s %6u %6u     %-25s %-25s %s\n", proto, rx, tx, local_addr, remote_addr, states[state]);
    }
}

void hex_to_ip(char* hex_ip, char* ip_str, int ip_str_size) { // converts hexadecimal to ip address by shifting bits
    unsigned long ip = strtoul(hex_ip, NULL, 16); // converts string to unsigned long
    snprintf(ip_str, ip_str_size, "%lu.%lu.%lu.%lu",
    ip & 0x000000FF, 
    (ip & 0x0000FF00) >> 8, 
    (ip & 0x00FF0000) >> 16, 
    (ip & 0xFF000000) >> 24);
}

void display_statistics(char* proto_label, struct StatMap proto_map[]) { // displays connection statistics
// open file
    int fd = open("/proc/net/snmp", O_RDONLY);
    if (fd < 0) {
        perror("Failed to open file.");
        exit(EXIT_FAILURE);
    }
// read file
    char buffer[2048];
    int readsize;
    readsize = read(fd, buffer, sizeof(buffer)-1);
    if (readsize <= 0) {
        perror("Failed to read file.");
        close(fd);
        exit(EXIT_FAILURE);
    }
// close file
    buffer[readsize] = '\0';
    close(fd); 
// print protocol label
    printf("%s\n", proto_label);
// parse file contents
    char* line = NULL;
    char* saveline = NULL;
    char* prev_line = strtok_r(buffer, "\n", &saveline);
    while ((line = strtok_r(NULL, "\n", &saveline)) != NULL) {
    // go through lines until label is found
        if (strncmp(prev_line, proto_label, strlen(proto_label)) == 0) {
        // parse through line
            char* savefield = NULL;
            char* savevalue = NULL;
            char* field = strtok_r(prev_line, " \n", &savefield);
            char* value = strtok_r(line, " \n", &savevalue);
            while (field && value) {
            // try to match field name in file to field name in mapping table
                for (int i = 0; proto_map[i].field_name != NULL; i++) {
                    if (strcmp(field, proto_map[i].field_name) == 0) {
                    // print line if correct field is identified
                        printf("\t%s %s\n", value, proto_map[i].format_string);
                        break;
                    }

                }
                field = strtok_r(NULL, " \n", &savefield);
                value = strtok_r(NULL, " \n", &savevalue);
            }
            return;
        }
        prev_line = line;
    }
} 
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <err.h>

#include "stub.h"

#define N_ARGS 4
#define N_ARGS_OPT 6
#define MAX_THREADS 600
#define COUNTER_FILE "server_output.txt"

typedef struct args {
    int port;
    enum modes priority;
    int ratio;
} * Args;

Args check_args(int argc, char *const *argv);

void usage() {
    fprintf(stderr, "usage: ./server --port PORT --priority writer/reader \
[--ratio N]\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *const *argv) {
    Args arguments = check_args(argc, argv);

    if (!load_config_server(arguments->port, arguments->priority, MAX_THREADS, 
                            COUNTER_FILE, arguments->ratio)) {
        free(arguments);
        exit(EXIT_FAILURE);                  
    }

    // Launch n threads with a maximum amount of 600
    while (1) {
        proccess_client();
    }

    free(arguments);
    close_config_server();

    return 0;
}

Args check_args(int argc, char *const *argv) {
    int c;
    int opt_index = 0;
    Args arguments = malloc(sizeof(struct args));

    if (arguments == NULL) err(EXIT_FAILURE, "Failed to allocate arguments");
    memset(arguments, 0, sizeof(struct args));

    arguments->ratio = -1;

    static struct option long_options[] = {
        {"port",        required_argument, 0,  0 },
        {"priority",    required_argument, 0,  0 },
        {"ratio",       required_argument, 0,  0 },
        {0,             0,                 0,  0 }
    };
    
    if (argc - 1 != N_ARGS && argc - 1 != N_ARGS_OPT) {
        free(arguments);
        usage();
    }

    while (1) {
        opt_index = 0;

        c = getopt_long(argc, argv, "", long_options, &opt_index);
        if (c == -1)
            break;

        switch (c) {
        case 0:
            if (strcmp(long_options[opt_index].name, "priority") == 0) {
                if (strcmp(optarg ,MODE_READER_STR) == 0) {
                    arguments->priority = READER;
                } else if (strcmp(optarg ,MODE_WRITER_STR) == 0) {
                    arguments->priority = WRITER;
                } else {
                    free(arguments);
                    usage();
                }
            } else if (strcmp(long_options[opt_index].name, "port") == 0) {
                arguments->port = atoi(optarg);
                if (arguments->port <= 0) {
                    free(arguments);
                    usage();
                }                
            } else if (strcmp(long_options[opt_index].name, "ratio") == 0) {
                arguments->ratio = atoi(optarg);
                if (arguments->ratio <= 0) {
                    free(arguments);
                    usage();
                }
                if (argc - 1 != N_ARGS_OPT) {
                    free(arguments);
                    usage();
                }
            }
            break;
        default:
            printf("?? getopt returned character code 0%o ??\n", c);
        }
    }

    if (optind < argc) {
        free(arguments);
        usage();
    }
    return arguments;
}

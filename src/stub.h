/**
 * @file stub.h
 * @author Javier Izquierdo Hernandez (j.izquierdoh.2021@alumnos.urjc.es)
 * @brief 
 * @version 0.1
 * @date 2023-11-10
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <err.h>
#include <string.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_IP_SIZE 16
#define MODE_READER_STR "reader"
#define MODE_WRITER_STR "writer"

// Modes for priority and mode selection
enum modes {
    WRITER = 0,
    READER
};

enum operations {
    WRITE = 0,
    READ
};

struct response {
    enum operations action;
    unsigned int counter;
    long latency_time;
};

int load_config_client(char ip[MAX_IP_SIZE], int port, int actions);

int load_config_server(int port, enum modes priority, int max_n_threads,
                       char * counter_file, int ratio);
int close_config_server();

void * client_connection(void * arg);
int proccess_client();
/**
 * @file proxy.c
 * @author Javier Izquierdo Hernandez (j.izquierdoh.2021@alumnos.urjc.es)
 * @brief 
 * @version 0.1
 * @date 2023-11-10
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "proxy.h"

#define ERROR(msg) fprintf(stderr,"PROXY ERROR: %s\n",msg)
#define NS_TO_S 1000000000
#define NS_TO_MICROS 1000
#define MICROS_TO_MS 1000
#define MAX_MS_SLEEP_INTERVAL 150
#define MIN_MS_SLEEP_INTERVAL 75

struct request {
    enum operations action;
    unsigned int id;
};

// ------------------------ Global variables for client ------------------------
int client_port;
int client_action;
char client_ip[MAX_IP_SIZE];
// ------------------------ Global variables for server ------------------------
int priority_server;
int server_sockfd;
struct sockaddr *server_addr;
socklen_t server_len;

struct thread_info {
    pthread_t thread;
    int sockfd;
};


sem_t sem;
pthread_mutex_t mutex;
pthread_mutex_t mutex2;
pthread_cond_t readers_reading;
pthread_cond_t writers_writing;
int counter = 0;
int n_writers = 0;
int n_readers = 0;
// -----------------------------------------------------------------------------

// ---------------------------- Initialize sockets -----------------------------
int load_config_client(char ip[MAX_IP_SIZE], int port, int action) {
    client_port = port;
    strcpy(client_ip, ip);
    client_action = action;

    return 1; 
}

int load_config_server(int port, enum modes priority, int max_n_threads) {
    const int enable = 1;
    struct sockaddr_in servaddr;
    int sockfd;
    socklen_t len;
    struct sockaddr * addr = malloc(sizeof(struct sockaddr));
    if (addr == NULL) err(EXIT_FAILURE, "failed to alloc memory");
    memset(addr, 0, sizeof(struct sockaddr));

    setbuf(stdout, NULL);

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    len = sizeof(servaddr);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        fprintf(stderr, "Socket failed\n");
        return 0;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
        &enable, sizeof(int)) < 0) {
        ERROR("Setsockopt(SO_REUSEADDR) failed");
        return 0;
    }

    if (bind(sockfd, (const struct sockaddr *) &servaddr, len) < 0) {
        ERROR("Unable to bind");
        return 0;
    }

    if (listen(sockfd, max_n_threads) < 0) {
        ERROR("Unable to listen");
        return 0;
    }

    if (sem_init(&sem, 0, max_n_threads) == -1) {
        err(EXIT_FAILURE, "failed to create semaphore");
    }

    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&mutex2, NULL);
    pthread_cond_init(&readers_reading, NULL);
    pthread_cond_init(&writers_writing, NULL);


    priority_server = priority;
    server_sockfd = sockfd;
    memcpy(addr, (struct sockaddr *) &servaddr, sizeof(struct sockaddr));
    server_addr = addr;
    server_len = len;

    return sockfd > 0;
}

int close_config_server() {
    close(server_sockfd);
    sem_destroy(&sem);
    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&mutex2);
    pthread_cond_destroy(&readers_reading);
    pthread_cond_destroy(&writers_writing);
    free(server_addr);
    return 1;
}
// -----------------------------------------------------------------------------

// --------------------------- Functions for client ----------------------------
void * client_connection(void * arg) {
    int id = *(int *) arg;
    struct in_addr addr;
    struct sockaddr_in servaddr;
    int sockfd;
    socklen_t len;
    struct response resp;
    struct request req;
    req.action = client_action;
    req.id = id;


    if (inet_pton(AF_INET, (char *) client_ip, &addr) < 0) {
        return NULL;
    }

    setbuf(stdout, NULL);

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = addr.s_addr;
    servaddr.sin_port = htons(client_port);

    len = sizeof(servaddr);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("Socket failed\n");
        return NULL;
    }

    if (connect(sockfd, (const struct sockaddr *) &servaddr, len) < 0){
        ERROR("Unable to connect");
        return NULL;
    }

    if (send(sockfd, &req, sizeof(req), MSG_WAITALL) < 0) ERROR("Fail to send");

    // Listen for response messages
    if (recv(sockfd, &resp, sizeof(resp), MSG_WAITALL) < 0) {
        ERROR("Fail to received");
        perror("Fail to receive");
        close(sockfd);
        pthread_exit(NULL);
    }
    switch (client_action) {
    case READER:
        printf("[Cliente #%d] Lector, contador=%d, tiempo=%ld ns.\n", id,
                resp.counter, resp.latency_time);
        break;
    case WRITER:
        printf("[Cliente #%d] Escritor, contador=%d, tiempo=%ld ns.\n", id,
                resp.counter, resp.latency_time);
        break;
    }


    close(sockfd);
    pthread_exit(NULL);
}
// ---------------------------- Function for server ----------------------------
void * proccess_client_thread(void * arg) {
    struct thread_info * thread_info = (struct thread_info *) arg;
    struct request req;
    struct response resp;
    struct timespec current_time, start, end;

    // Listen for response messages
    if (recv(thread_info->sockfd, &req, sizeof(req), MSG_WAITALL) < 0) {
        ERROR("Fail to received");
        free(thread_info);
        sem_post(&sem);
        pthread_exit(NULL);
    }

    clock_gettime(CLOCK_REALTIME, &current_time);
    switch (req.action)
    {
    case WRITE:
        clock_gettime(CLOCK_MONOTONIC, &start);
        pthread_mutex_lock(&mutex);
        clock_gettime(CLOCK_MONOTONIC, &end);
        if (priority_server == WRITER) {
            // We have priority, we stop readers
        } else if (priority_server == READER) {
            // Check if we do not have readers
            while (n_readers > 0) {
                pthread_cond_wait(&readers_reading, &mutex);
            }
        }
        counter++;
        printf("[%ld.%.6ld][ESCRITOR #%d] modifica contador con valor %d\n",
                current_time.tv_sec, current_time.tv_nsec / NS_TO_MICROS,
                req.id, counter);
        resp.action = req.action;
        resp.counter = counter;
        resp.latency_time = end.tv_nsec - start.tv_nsec;

        // Sleep between 150 and 75 miliseconds
        usleep((rand() % (MAX_MS_SLEEP_INTERVAL - MIN_MS_SLEEP_INTERVAL)
            + MIN_MS_SLEEP_INTERVAL) * MICROS_TO_MS);
        n_readers--;
        pthread_cond_signal(&writers_writing);
        pthread_mutex_unlock(&mutex);
        break;
    case READ:
        clock_gettime(CLOCK_MONOTONIC, &start);
        pthread_mutex_lock(&mutex2);
        clock_gettime(CLOCK_MONOTONIC, &end);
        if (priority_server == WRITER) {
            // Check if we do not have writers
            while (n_writers > 0) {
                pthread_cond_wait(&writers_writing, &mutex2);
            }
        } else if (priority_server == READER) {
            // We have priority, we stop writers
        }
        n_readers++;
        printf("[%ld.%.6ld][LECTOR #%d] modifica contador con valor %d\n",
                current_time.tv_sec, current_time.tv_nsec / NS_TO_MICROS,
                req.id, counter);
        resp.action = req.action;
        resp.counter = counter;
        resp.latency_time = end.tv_nsec - start.tv_nsec;

        // Sleep between 150 and 75 miliseconds
        usleep((rand() % (MAX_MS_SLEEP_INTERVAL - MIN_MS_SLEEP_INTERVAL)
            + MIN_MS_SLEEP_INTERVAL) * MICROS_TO_MS);
        n_readers--;
        pthread_cond_signal(&readers_reading);
        pthread_mutex_unlock(&mutex2);
        break;
    }

    if (resp.latency_time < 0) {
        resp.latency_time = NS_TO_S - resp.latency_time;
    }

    if (send(thread_info->sockfd, &resp, sizeof(resp), MSG_WAITALL) < 0) {
        ERROR("Failed to send");
    }

    free(thread_info);
    sem_post(&sem);
    close(thread_info->sockfd);
    pthread_exit(NULL);
}

int proccess_client() {
    int sockfd; 
    // struct sockaddr * addr = server_addr;
    struct thread_info * thread_info = malloc(sizeof(struct thread_info));
    if (thread_info == NULL) err(EXIT_FAILURE, "failed to alloc memory");
    memset(thread_info, 0, sizeof(struct thread_info));

    if ((sockfd = accept(server_sockfd, server_addr, &server_len)) < 0) {
        ERROR("failed to accept socket");
        return 0;
    }

    // 1. Accept connection
    thread_info->sockfd = sockfd;
    sem_wait(&sem);
    pthread_create(&thread_info->thread, NULL, proccess_client_thread, thread_info);
    pthread_detach(thread_info->thread);

    return 1;
}
// -----------------------------------------------------------------------------
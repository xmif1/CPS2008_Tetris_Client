#include "client_server.h"

// return >= 0 on success, return being the socket created for client; else return -1 on failure.
int client_init(){
    int socket_fd;
    struct sockaddr_in serveraddrIn = {.sin_family = SDOMAIN, .sin_addr.s_addr = inet_addr(IP_LOCALHOST),
            .sin_port = htons(PORT)};

    // Create socket
    if((socket_fd = socket(SDOMAIN, TYPE, 0)) < 0){
        return -1;
    }

    // Then connect it...
    if(connect(socket_fd, (struct sockaddr*) &serveraddrIn, sizeof(serveraddrIn)) < 0){
        return -1;
    }

    pthread_t server_thread;
    if(pthread_create(&server_thread, NULL, server_listen, (void*) &socket_fd) != 0){
        return -1;
    }

    return socket_fd;
}

void* server_listen(void* arg){
    int* socket_fd_ptr;
    socket_fd_ptr = (int*) arg;

    char recv_msg[BUFFER_SIZE];
    while(recv(*socket_fd_ptr, recv_msg, BUFFER_SIZE, 0) > 0){
        enqueue(recv_msg);
    }
}

void enqueue(char recv_msg[BUFFER_SIZE]){
    while(1){
        if(n_recv_msgs < (MSG_BUFFER_SIZE - 1)){ // if msg buffer is full, further buffering is handled by TCPs flow control
            pthread_mutex_lock(&recv_msgs_mutex); pthread_mutex_lock(&n_recv_msgs_mutex);
            strcpy(recv_msgs[n_recv_msgs], recv_msg);
            n_recv_msgs++;
            pthread_mutex_unlock(&recv_msgs_mutex); pthread_mutex_unlock(&n_recv_msgs_mutex);

            break;
        }
    }
}

void dequeue(char* msg){
    msg = NULL;
    if(n_recv_msgs > 0){
        pthread_mutex_lock(&recv_msgs_mutex); pthread_mutex_lock(&n_recv_msgs_mutex);
        strcpy(msg, recv_msgs[n_recv_msgs - 1]);
        n_recv_msgs--;
        pthread_mutex_unlock(&recv_msgs_mutex); pthread_mutex_unlock(&n_recv_msgs_mutex);
    }
}

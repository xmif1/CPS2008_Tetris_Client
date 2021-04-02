#include "../include/client_server.h"

// return >= 0 on success, return being the socket created for client; else return -1 on failure.
int client_init(){
    int socket_fd;
    struct sockaddr_in serveraddrIn = {.sin_family = SDOMAIN, .sin_addr.s_addr = inet_addr(IP_LOCALHOST),
            .sin_port = htons(PORT)};

    // Create socket
    socket_fd = socket(SDOMAIN, TYPE, 0);
    if(socket_fd < 0){
        return -1;
    }

    // Then connect it...
    if(connect(socket_fd, (struct sockaddr*) &serveraddrIn, sizeof(serveraddrIn)) < 0){
        return -1;
    }

    return socket_fd;
}

void enqueue_msgs(int socket_fd){
    char recv_msg[BUFFER_SIZE];
    while(recv(socket_fd, recv_msg, BUFFER_SIZE, 0) > 0){
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
}

void dequeue_msgs(char* msg){
    if(n_recv_msgs > 0){
        pthread_mutex_lock(&recv_msgs_mutex); pthread_mutex_lock(&n_recv_msgs_mutex);

        msg = malloc(sizeof(recv_msgs[n_recv_msgs - 1])+1);
        strcpy(msg, recv_msgs[n_recv_msgs - 1]);
        n_recv_msgs--;

        pthread_mutex_unlock(&recv_msgs_mutex); pthread_mutex_unlock(&n_recv_msgs_mutex);
    }
}

int send_msg(char* msg, int socket_fd){
    if(send(socket_fd, msg, strlen(msg), 0) < 0){
        return -1;
    }else{
        return 0;
    }
}

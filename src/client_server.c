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

    // Set timeout of recv
    struct timeval timeout;
    timeout.tv_sec = 5; // timeout after 5 seconds
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*) &timeout, sizeof(timeout));

    // Then connect it...
    if(connect(socket_fd, (struct sockaddr*) &serveraddrIn, sizeof(serveraddrIn)) < 0){
        return -1;
    }

    return socket_fd;
}

void enqueue_msg(int socket_fd){
    msg recv_msg;
    if(recv(socket_fd, (void*) &recv_msg, sizeof(msg), 0) > 0){
        if(recv_msg.msg_type == CHAT){
            while(1){
                // if msg buffer is full, further buffering is handled by TCPs flow control
                if(n_chat_msgs < (MSG_BUFFER_SIZE - 1)){
                    pthread_mutex_lock(&threadMutex);
                    recv_chat_msgs[n_chat_msgs] = recv_msg;
                    n_chat_msgs++;
                    pthread_mutex_unlock(&threadMutex);

                    break;
                }
            }
        }
        // will later handle different types of msgs
    }
}

msg dequeue_chat_msg(){
    msg recv_msg;
    while(1){
        if(n_chat_msgs > 0){
            pthread_mutex_lock(&threadMutex);
            recv_msg = recv_chat_msgs[n_chat_msgs - 1];
            n_chat_msgs--;
            pthread_mutex_unlock(&threadMutex);

            break;
        }
    }

    return recv_msg;
}

int send_msg(msg send_msg, int socket_fd){
    if(send(socket_fd, (void*) &send_msg, sizeof(msg), 0) < 0){
        return -1;
    }else{
        return 0;
    }
}

/* ----------- ERROR HANDLING ----------- */

/* Mr. Error: A simple function to handle errors (mostly a wrapper to perror), and terminate.*/
void mrerror(char* err_msg){
    red();
    perror(err_msg);
    reset();
    exit(EXIT_FAILURE);
}

/* Silent Mr. Error: A simple function to handle errors (mostly a wrapper to perror), without termination.*/
void smrerror(char* err_msg){
    yellow();
    perror(err_msg);
    reset();
}

// The following functions are used to highlight text outputted to the console, for reporting errors and warnings.

void red(){
    printf("\033[1;31m");
}

void yellow(){
    printf("\033[1;33m");
}

void reset(){
    printf("-----");
    printf("\033[0m\n");
}

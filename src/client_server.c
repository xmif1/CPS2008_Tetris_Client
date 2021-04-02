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

void enqueue_msg(int socket_fd){
    msg recv_msg;
    if(recv(socket_fd, (void*) &recv_msg, sizeof(msg), 0) > 0){
      while(1){
            if(n_recv_msgs < (MSG_BUFFER_SIZE - 1)){ // if msg buffer is full, further buffering is handled by TCPs flow control
                pthread_mutex_lock(&recv_msgs_mutex);
                recv_msgs[n_recv_msgs] = recv_msg;
                n_recv_msgs++;
                pthread_mutex_unlock(&recv_msgs_mutex);

                break;
            }
        }
    }
}

msg dequeue_msg(){
    msg recv_msg = {.msg = "", .msg_type = EMPTY};
    if(n_recv_msgs > 0){
        pthread_mutex_lock(&recv_msgs_mutex);
        recv_msg = recv_msgs[n_recv_msgs - 1];
        n_recv_msgs--;
        pthread_mutex_unlock(&recv_msgs_mutex);
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

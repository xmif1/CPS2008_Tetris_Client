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

int enqueue_msg(int socket_fd){
    msg recv_msg;
    int tbr, recv_str_len;
    char header[HEADER_SIZE];

    fd_set set; FD_ZERO(&set); FD_SET(socket_fd, &set);

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    int ret = select(socket_fd + 1, &set, NULL, NULL, &timeout);

    if(ret < 0){
        return ret;
    }
    else if(ret == 0){
        return 1;
    }
    else if((ret = recv(socket_fd, (void*) &header, HEADER_SIZE, 0)) > 0){
        int recv_header_failed = 0;
        for(tbr = ret; tbr < HEADER_SIZE; tbr += ret){
            if((ret = recv(socket_fd, (void*) (&header + tbr), recv_str_len - tbr, 0)) < 0){
                recv_header_failed = 1;
                break;
            }
        }

        if(!recv_header_failed){
            int recv_msg_failed = 0;
            char* token = strtok(header, "::");
            recv_str_len = strtol(token, NULL, 10);

            token = strtok(NULL, "::");
            recv_msg.msg_type = strtol(token, NULL, 10);

            recv_msg.msg = malloc(recv_str_len);
            if(recv_msg.msg == NULL){
                mrerror("Error while allocating memory");
            }

            for(tbr = 0; tbr < recv_str_len; tbr += ret){
                if((ret = recv(socket_fd, (void*) recv_msg.msg + tbr, recv_str_len - tbr, 0)) < 0){
                    recv_msg_failed = 1;
                    break;
                }
            }

            if(!recv_msg_failed){
                switch(recv_msg.msg_type){
                    case CHAT: handle_chat_msg(recv_msg); break;
                }
                // will later handle different types of msgs
            }
        }
    }

    return ret;
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
    int msg_len = strlen(send_msg.msg) + 1;
    int str_to_send_len = HEADER_SIZE + msg_len;
    char header[HEADER_SIZE];
    char* str_to_send = malloc(str_to_send_len);

    if(str_to_send == NULL){
        mrerror("Failed to allocate memory for message send");
    }

    int i = 0;
    for(; i < MSG_LEN_DIGITS - ((int) floor(log10(msg_len)) + 1); i++){
        header[i] = '0';
    }

    sprintf(header + i, "%d", msg_len);
    strcat(header, "::");
    sprintf(header + MSG_LEN_DIGITS + 2, "%d", send_msg.msg_type);
    strcat(header, "::");

    strcpy(str_to_send, header);
    strcat(str_to_send, send_msg.msg);
    str_to_send[str_to_send_len-1] = '\0'; // ensure null terminated

    int tbs; // tbs = total bytes sent
    int sent_bytes;

    for(tbs = 0; tbs < str_to_send_len; tbs += sent_bytes){
        if((sent_bytes = send(socket_fd, (void*) str_to_send + tbs, str_to_send_len - tbs, 0)) < 0){
            break;
        }
    }

    free(str_to_send);

    return sent_bytes;
}

/* ----------- UTIL FUNCTIONS ----------- */

void handle_chat_msg(msg recv_msg){
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

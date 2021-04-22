#include "../include/client_server.h"

// wrapper around client connect, used to connect with server
int client_init(){
    server_fd = client_connect(IP_LOCALHOST, PORT);

    return server_fd;
}

// return >= 0 on success, return being the socket created for client; else return -1 on failure.
int client_connect(char ip[INET_ADDRSTRLEN], int port){
    int socket_fd;
    struct sockaddr_in serveraddrIn = {.sin_family = SDOMAIN, .sin_addr.s_addr = inet_addr(ip), .sin_port = htons(port)};

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

msg* recv_msg(int socket_fd){
    msg* recv_msg = malloc(sizeof(msg));
    int ret, tbr, recv_str_len; // tbr = total bytes read
    char header[HEADER_SIZE]; header[HEADER_SIZE - 1] = '\0';

    if((ret = recv(socket_fd, (void*) &header, HEADER_SIZE - 1, 0)) > 0){
        for(tbr = ret; tbr < HEADER_SIZE - 1; tbr += ret){
            if((ret = recv(socket_fd, (void*) (&header + tbr), HEADER_SIZE - tbr, 0)) < 0){
                return NULL;
            }
        }

        char* token = strtok(header, "::");
        recv_str_len = strtol(token, NULL, 10);

        token = strtok(NULL, "::");
        recv_msg->msg_type = strtol(token, NULL, 10);

        recv_msg->msg = malloc(recv_str_len);

        for(tbr = 0; tbr < recv_str_len; tbr += ret){
            if((ret = recv(socket_fd, (void*) recv_msg->msg + tbr, recv_str_len - tbr, 0)) < 0){
                return NULL;
            }
        }

        return recv_msg;
    }

    return NULL;
}

int enqueue_server_msg(int socket_fd){
    msg* recv_server_msg;
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
    else if((recv_server_msg = recv_msg(socket_fd)) != NULL){
        switch(recv_server_msg->msg_type){
            case CHAT: handle_chat_msg(*recv_server_msg); break;
        }
        // will later handle different types of msgs
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

int send_msg(msg sendMsg, int socket_fd){
    int msg_len = strlen(sendMsg.msg) + 1;
    int str_to_send_len = HEADER_SIZE + msg_len - 1;
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
    sprintf(header + MSG_LEN_DIGITS + 2, "%d", sendMsg.msg_type);
    strcat(header, "::");

    strcpy(str_to_send, header);
    strcat(str_to_send, sendMsg.msg);
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

void handle_chat_msg(msg recvMsg){
    while(1){
        // if msg buffer is full, further buffering is handled by TCPs flow control
        if(n_chat_msgs < (MSG_BUFFER_SIZE - 1)){
            pthread_mutex_lock(&threadMutex);
            recv_chat_msgs[n_chat_msgs] = recvMsg;
            n_chat_msgs++;
            pthread_mutex_unlock(&threadMutex);

            break;
        }
    }
}

void handle_new_game_msg(msg recvMsg){
    char* token = strtok(recvMsg.msg, "::");
    gameSession.game_type = strtol(token, NULL, 10);

    token = strtok(NULL, "::");
    gameSession.n_baselines = strtol(token, NULL, 10);

    token = strtok(NULL, "::");
    gameSession.n_winlines = strtol(token, NULL, 10);

    token = strtok(NULL, "::");
    gameSession.time = strtol(token, NULL, 10);

    token = strtok(NULL, "::");
    int port = PORT + strtol(token, NULL, 10);

    gameSession.n_players = 0;
    token = strtok(NULL, "::");
    while(token != NULL){
        if(port != (PORT + gameSession.n_players + 1)){
            gameSession.players[gameSession.n_players] = malloc(sizeof(ingame_client));
            gameSession.players[gameSession.n_players]->state = WAITING;
            gameSession.players[gameSession.n_players]->port = PORT + gameSession.n_players + 1;
            strcpy(gameSession.players[gameSession.n_players]->ip, token);

            gameSession.n_players++;
        }

        token = strtok(NULL, "::");
    }

    // Create socket
    int p2p_fd = socket(SDOMAIN, TYPE, 0);
    if(p2p_fd < 0){
        mrerror("Peer-to-peer socket initialisation failed");
    }

    struct sockaddr_in sockaddrIn = {.sin_family = SDOMAIN, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(port)};

    // Then (in darkness) bind it...
    if(bind(p2p_fd, (struct sockaddr*) &sockaddrIn, sizeof(sockaddrIn)) < 0){
        mrerror("Peer-to-peer socket binding failed");
    }

    // Finally, listen.
    if(listen(p2p_fd, 5) < 0){
        mrerror("Listening on peer-to-peer socket failed");
    }

    gameSession.p2p_fd = p2p_fd;

    msg sendMsg;
    sendMsg.msg_type = P2P_READY;
    sendMsg.msg = malloc(1);
    strcpy(sendMsg.msg, "");

    send_msg(sendMsg, server_fd);
}

// notice that data accessed by this function running in its own thread is independent from the data accessed by the
// join_peer_connections function running in its own thread; in this manner, these functions are thread safe
void* accept_peer_connections(void* arg){
    struct sockaddr_in clientaddrIn;
    socklen_t sizeof_clientaddrIn = sizeof(struct sockaddr_in);
    fd_set recv_fds;
    int nfds;

    int n_connected_players = 0;
    for(int i = 0; i < gameSession.n_players; i++){
        gameSession.players[i]->client_fd = 0;
    }

    int select_ret = 0;
    while(1){
        FD_ZERO(&recv_fds);
        FD_SET(gameSession.p2p_fd, &recv_fds);
        nfds = gameSession.p2p_fd;

        for(int i = 0; i < gameSession.n_players; i++){
            pthread_mutex_lock(clientMutexes + i);
            if(gameSession.players[i]->state != DISCONNECTED || gameSession.players[i]->state != FINISHED){
                if(gameSession.players[i]->client_fd > 0){
                    FD_SET(gameSession.players[i]->client_fd, &recv_fds);
                }

                if(nfds < gameSession.players[i]->client_fd){
                    nfds = gameSession.players[i]->client_fd;
                }
            }
            pthread_mutex_unlock(clientMutexes + i);
        }

        if(select(nfds + 1, &recv_fds, NULL, NULL, NULL) < 0){
            mrerror("Error occured on select between client connections");
        }
        else if(FD_ISSET(gameSession.p2p_fd, &recv_fds) && n_connected_players < gameSession.n_players){
            int client_fd = accept(gameSession.p2p_fd, (struct sockaddr*) &clientaddrIn, &sizeof_clientaddrIn);

            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientaddrIn.sin_addr, ip, INET_ADDRSTRLEN);

            for(int i = 0; i < gameSession.n_players; i++){
                if(strcmp(gameSession.players[i]->ip, ip) == 0){
                    gameSession.players[i]->state = CONNECTED;
                    gameSession.players[i]->client_fd = client_fd;

                    n_connected_players++;
                    break;
                }
            }

            if(gameSession.n_players == n_connected_players){
                msg sendMsg;
                sendMsg.msg_type = CLIENTS_CONNECTED;
                sendMsg.msg = malloc(1);
                strcpy(sendMsg.msg, "");

                for(int i = 0; i < gameSession.n_players; i++){
                    if(send_msg(sendMsg, gameSession.players[i]->server_fd) < 0){
                        pthread_mutex_lock(clientMutexes + i);
                        gameSession.players[i]->state = DISCONNECTED;
                        close(gameSession.players[i]->client_fd); gameSession.players[i]->client_fd = 0;
                        close(gameSession.players[i]->server_fd); gameSession.players[i]->server_fd = 0;
                        pthread_mutex_unlock(clientMutexes + i);
                    }
                }
            }
        }else if(n_connected_players == gameSession.n_players){
            continue;
        }
    }
}

// notice that data accessed by this function running in its own thread is independent from the data accessed by the
// accept_peer_connections function running in its own thread; in this manner, these functions are thread safe
void* service_peer_connections(void* arg){
    for(int i = 0; i < gameSession.n_players; i++){
        int client_server_fd = client_connect(gameSession.players[i]->ip, gameSession.players[i]->port);
        if(server_fd < 0){
            pthread_mutex_lock(clientMutexes + i);
            gameSession.players[i]->state = DISCONNECTED;
            close(gameSession.players[i]->client_fd); gameSession.players[i]->client_fd = 0;
            close(gameSession.players[i]->server_fd); gameSession.players[i]->server_fd = 0;
            pthread_mutex_unlock(clientMutexes + i);
        }
        else{
            gameSession.players[i]->server_fd = client_server_fd;
        }
    }

    fd_set recv_fds;
    int n_ack_recv, select_ret, nfds;
    n_ack_recv = 0;

    while(n_ack_recv < gameSession.n_players - 1){
        FD_ZERO(&recv_fds);
        FD_SET(gameSession.p2p_fd, &recv_fds);
        select_ret = nfds = 0;

        for(int i = 0; i < gameSession.n_players; i++){
            pthread_mutex_lock(clientMutexes + i);
            if(gameSession.players[i]->state != DISCONNECTED || gameSession.players[i]->state != FINISHED){
                if(gameSession.players[i]->server_fd > 0){
                    FD_SET(gameSession.players[i]->server_fd, &recv_fds);
                }

                if(nfds < gameSession.players[i]->server_fd){
                    nfds = gameSession.players[i]->server_fd;
                }
            }
            pthread_mutex_unlock(clientMutexes + i);
        }

        if(select(nfds + 1, &recv_fds, NULL, NULL, NULL) < 0){
            mrerror("Error occured on select between server connections");
        }else{
            for(int i = 0; i < gameSession.n_players; i++){
                if(FD_ISSET(gameSession.players[i]->server_fd, &recv_fds)){
                    msg* recv_server_msg;

                    if(((recv_server_msg = recv_msg(gameSession.players[i]->server_fd)) != NULL)
                            && (recv_server_msg->msg_type == CLIENTS_CONNECTED)){

                        n_ack_recv++;
                    }else{
                        pthread_mutex_lock(clientMutexes + i);
                        gameSession.players[i]->state = DISCONNECTED;
                        close(gameSession.players[i]->client_fd); gameSession.players[i]->client_fd = 0;
                        close(gameSession.players[i]->server_fd); gameSession.players[i]->server_fd = 0;
                        pthread_mutex_unlock(clientMutexes + i);
                    }
                }
            }
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

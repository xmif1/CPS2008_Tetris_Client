#include "../include/client_server.h"

/* Initialiser for a new client instance, in particular responsible for:
 * (i) connecting to the game server by calling the client_server library function
 * (ii) initialising the mutexes for the P2P clients array
 *
 * If client_connect fails to open and connect a socket, -1 is returned. This return is propogated by client_init and
 * its the resonsibility of the caller to handle this accordingly.
 */
int client_init(){
    // initialise mutexes for P2P clients
    for(int i = 0; i < N_SESSION_PLAYERS; i++){
        pthread_mutex_init(&clientMutexes[i], NULL);
    }

    // connect to the game server and set global file descriptor reference to returned fd by client_connect
    server_fd = client_connect(IP_LOCALHOST, PORT); // returns -1 on failure
    return server_fd; // propogate the return of client_connect...
}

/* Library function for connecting a client at a specified IPv4 address and port, initialising a socket and attempt to
 * connect. We also attempt to set socket reuse at the specified port, to mitigate the port allocation issue outlined in
 * the report.
 *
 * The function returns >= 0 (the socket file descriptor) on success, -1 on failure.
 */
int client_connect(char ip[INET_ADDRSTRLEN], int port){
    int socket_fd;
    struct sockaddr_in serveraddrIn = {.sin_family = SDOMAIN, .sin_addr.s_addr = inet_addr(ip), .sin_port = htons(port)};

    // Create socket
    socket_fd = socket(SDOMAIN, TYPE, 0);
    if(socket_fd < 0){
        return -1; // return -1 on failure
    }

    // (attempt to) allow port reuse...
    if(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0){
	    return -1; // return -1 on failure
    }

    // Then connect it...
    if(connect(socket_fd, (struct sockaddr*) &serveraddrIn, sizeof(serveraddrIn)) < 0){
        return -1; // return -1 on failure
    }

    return socket_fd; // otherwise if socket initialised and connected, return...
}

/* Library function for fetching an encoded message from the specified socket. The message is decoded accordingly and
 * represented as a msg struct. The encoding format and decoding procedure is outlined accordingly in the project report.
 *
 * Returns the msg instance to the caller. In case of a failure during message receipt, the msg instance is still returned
 * but the msg type is set to INVALID.
 */
msg recv_msg(int socket_fd){
    msg recv_msg;
    recv_msg.msg_type = INVALID;

    // variables used to maintain total bytes read (tbr), number of bytes received from last call to recv, and the
    // expected length of the next data part
    int ret, tbr, recv_str_len; // tbr = total bytes read

    // initialise char array to keep header of message
    char header[HEADER_SIZE]; header[HEADER_SIZE - 1] = '\0';

    // initial call to recv attempts to fetch the header of the message first
    if((ret = recv(socket_fd, (void*) &header, HEADER_SIZE - 1, 0)) > 0){
        // ensure that the header is recieved entirely (keep on looping until tbr == HEADER_SIZE - 1)
        for(tbr = ret; tbr < HEADER_SIZE - 1; tbr += ret){
            if((ret = recv(socket_fd, (void*) (&header + tbr), HEADER_SIZE - tbr, 0)) < 0){
                // if erroneous i.e. we have not managed to fetch a complete message header, set type as INVALID to
                // signal to calling function
                recv_msg.msg_type = INVALID;
            }
        }

        // decode the header by extracting the expected length of the data part and the message type
	    char str_len_part[5]; strncpy(str_len_part, header, 4); str_len_part[4] = '\0';
        recv_str_len = strtol(str_len_part, NULL, 10);
        recv_msg.msg_type = header[6] - '0';

        // initialise array of decoded data part length, in which data part will be stored
        recv_msg.msg = malloc(recv_str_len);
        if(recv_msg.msg == NULL){
            mrerror("Error while allocating memory");
        }

        // reset tbr to 0, loop until the successive calls to recv yield the entire data part
        for(tbr = 0; tbr < recv_str_len; tbr += ret){
            if((ret = recv(socket_fd, (void*) recv_msg.msg + tbr, recv_str_len - tbr, 0)) < 0){
                // if erroneous i.e. we have not managed to fetch a complete data part, set type as INVALID to signal to
                // calling function
                recv_msg.msg_type = INVALID;
            }
        }
    }

    return recv_msg;
}
/* Library function for fetching a message from the specified socket, by first selecting on the socket with a timeout.
 * If a socket has data to stream, this is fetched using a call to recv_msg outlined earlier, and enqueing the message
 * in recv_server_msgs buffer. In essence then, enqueue_server_msg is a time-out variant of recv_msg.
 */
msg enqueue_server_msg(int socket_fd){
    // Define file descriptor set on which to carry out select; in essence contains socket_fd only
    fd_set set; FD_ZERO(&set); FD_SET(socket_fd, &set);

    // Define time out for select, set for 5 seconds
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    int ret = select(socket_fd + 1, &set, NULL, NULL, &timeout); // select on file descriptor set with time out

    // if ret < 0, then server has disconnected and we return a msg of type INVALID to signal disconnection to the caller
    if(ret < 0){
        msg err_msg; err_msg.msg_type = INVALID;
        return err_msg;
    }
    else if(ret == 0){ // else if ret = 0, then no data is available from the server after the timeout
        // we then send a msg to type EMPTY to the caller, to signal that server is still connected but no data is available
        msg empty_msg; empty_msg.msg_type = EMPTY;
        return empty_msg;
    }else{
        // else data is available and we fetch it via a call to recv_msg
        msg recvMsg = recv_msg(socket_fd);

        while(1){ // wait until we can enqueue the msgs
            // note that if msg buffer is full, TCP flow control kicks in, since it is a streaming protocol...
            if(n_server_msgs < (MSG_BUFFER_SIZE - 1)){
                // since the message buffer is also accessed to retrieve messages, possibly by a different thread, we
                // must ensure that it is accessed in a thread--safe manner.
                pthread_mutex_lock(&threadMutex); // obtain mutex lock for thread of execution
                recv_server_msgs[n_server_msgs] = recvMsg; // enqueue in order
                n_server_msgs++; // update queue size
                pthread_mutex_unlock(&threadMutex); // release mutex lock for thread of execution

		        break;
            }
        }

        return recvMsg;
    }
}

/* Library function for returning a msg instance from the recv_server_msgs buffer, in the order received, and in a
 * thread-safe manner. If queue is empty, a msg of type EMPTY is returned.
 */
msg dequeue_server_msg(){
    msg recv_msg; recv_msg.msg_type = EMPTY;

    if(n_server_msgs > 0){ //
        pthread_mutex_lock(&threadMutex); // obtain mutex lock for thread of execution
        recv_msg = recv_server_msgs[n_server_msgs - 1]; // dequeue in order
        n_server_msgs--; // update queue size
        pthread_mutex_unlock(&threadMutex); // release mutex lock for thread of execution

    }

    return recv_msg;
}

/* Library function used to send a message at the specified socket, taking care of encoding the message (as described in
 * detail in the project report), ensuring that the entire message is sent, and carrying out suitable error checks and
 * handling. Returns the number of sent bytes to the caller; if the return is negative, then an error has occured on
 * send, and should typically follow by disconnection.
 */
int send_msg(msg sendMsg, int socket_fd){
    // initialise necessary variables for encoding the message
    int msg_len = strlen(sendMsg.msg) + 1;
    int str_to_send_len = HEADER_SIZE + msg_len - 1; // enough space for the header + data part + null character
    char header[HEADER_SIZE];
    char* str_to_send = malloc(str_to_send_len);

    // if allocation of memory for holding the message to send failed, report an error and exit
    if(str_to_send == NULL){
        mrerror("Failed to allocate memory for message send");
    }

    // the header must always be of fixed size, with the data part length having MSG_LEN_DIGITS; if the required number
    // of digits is less than MSG_LEN_DIGITS, we prepend the required number of 0s to the header
    int i = 0;
    for(; i < MSG_LEN_DIGITS - ((int) floor(log10(msg_len)) + 1); i++){
        header[i] = '0';
    }

    sprintf(header + i, "%d", msg_len); // concat the data part length
    strcat(header, "::"); // concat the separation token
    sprintf(header + MSG_LEN_DIGITS + 2, "%d", sendMsg.msg_type); // concat the message type
    strcat(header, "::"); // concat the separation token

    strcpy(str_to_send, header); // copy the header to the string holding the final message string to be sent
    strcat(str_to_send, sendMsg.msg); // append the data part to this string
    str_to_send[str_to_send_len-1] = '\0'; // ensure null terminated

    int tbs; // tbs = total bytes sent
    int sent_bytes;

    // make successive calls to send() until the entire message is sent or an error occurs
    for(tbs = 0; tbs < str_to_send_len; tbs += sent_bytes){
        if((sent_bytes = send(socket_fd, (void*) str_to_send + tbs, str_to_send_len - tbs, 0)) < 0){
            break; // in case of error, sent_bytes < 0 and hence a -ve value is returned indicating an error
        }
    }

    free(str_to_send); // free memory as necessary

    return sent_bytes;
}

/* ----------- UTIL FUNCTIONS ----------- */

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
    gameSession.seed = strtol(token, NULL, 10);

    token = strtok(NULL, "::");
    int port = PORT + strtol(token, NULL, 10);

    gameSession.score = 0;
    gameSession.game_in_progress = 1;
    gameSession.n_lines_to_add = 0;
    gameSession.total_lines_cleared = 0;
    time(&gameSession.start_time);

    gameSession.n_players = 0;
    token = strtok(NULL, "::");

    int offset = 1;
    while(token != NULL){
        if(port != (PORT + offset)){
            gameSession.players[gameSession.n_players] = malloc(sizeof(ingame_client));
            gameSession.players[gameSession.n_players]->state = WAITING;
            gameSession.players[gameSession.n_players]->port = PORT + offset;
            strcpy(gameSession.players[gameSession.n_players]->ip, token);

            gameSession.n_players++;
        }

        offset++;
        token = strtok(NULL, "::");
    }

    if(gameSession.game_type != CHILL){
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
        strcpy(sendMsg.msg, "\0");

        send_msg(sendMsg, server_fd);
    }
}

int end_game(){
    msg finished_msg;
    finished_msg.msg_type = FINISHED_GAME;
    finished_msg.msg = malloc(1);
    strcpy(finished_msg.msg, "\0");

    pthread_mutex_lock(&gameMutex);
    for(int i = 0; i < gameSession.n_players; i++){
        if(gameSession.players[i]->state != DISCONNECTED || gameSession.players[i]->state != FINISHED){
            send_msg(finished_msg, gameSession.players[i]->server_fd);
            if(gameSession.players[i]->server_fd > 0){ close(gameSession.players[i]->server_fd);}
            if(gameSession.players[i]->client_fd > 0){ close(gameSession.players[i]->client_fd);}
        }

        free(gameSession.players[i]);
    }

    if(gameSession.game_type != CHILL){
        close(gameSession.p2p_fd);
    }
    gameSession.p2p_fd = 0;

    // get final score
    msg score_msg;
    score_msg.msg_type = SCORE_UPDATE;
    score_msg.msg = malloc(7);
    if(score_msg.msg == NULL){
        mrerror("Failed to allocate memory for score update to server");
    }
    sprintf(score_msg.msg, "%d", gameSession.score);

    gameSession.game_in_progress = 0;
    pthread_mutex_unlock(&gameMutex);

    // send final score update to server
    send_msg(score_msg, server_fd);

    // signal to server that player is finished
    return send_msg(finished_msg, server_fd);
}

void send_cleared_lines(int n_cleared_lines){
    msg lines_msg;
    lines_msg.msg_type = LINES_CLEARED;
    lines_msg.msg = malloc(4);
    sprintf(lines_msg.msg, "%d", n_cleared_lines);

    pthread_mutex_lock(&gameMutex);
    for(int i = 0; i < gameSession.n_players; i++){
        if(gameSession.players[i]->state != DISCONNECTED || gameSession.players[i]->state != FINISHED){
            send_msg(lines_msg, gameSession.players[i]->server_fd);
        }
    }
    pthread_mutex_unlock(&gameMutex);
}

int get_score(){
    pthread_mutex_lock(&gameMutex);

    int score = -1;
    if(gameSession.game_in_progress){
        score = gameSession.score;
    }

    pthread_mutex_unlock(&gameMutex);

    return score;
}

void set_score(int score){
    pthread_mutex_lock(&gameMutex);

    if(gameSession.game_in_progress){
        gameSession.score = score;
    }

    pthread_mutex_unlock(&gameMutex);
}

int get_lines_to_add(){
    pthread_mutex_lock(&gameMutex);

    int n_lines = -1;
    if(gameSession.game_in_progress){
        n_lines = gameSession.n_lines_to_add;
	gameSession.n_lines_to_add = 0;
    }

    pthread_mutex_unlock(&gameMutex);

    return n_lines;
}

int signalGameTermination(){
   int ret = 0;

    pthread_mutex_lock(&gameMutex);
    if(gameSession.game_in_progress){
	ret = 1;
        gameSession.game_in_progress = 0;
    }
    pthread_mutex_unlock(&gameMutex);

    return ret;
}

// ------ THREADED FRONT-END FUNCTIONS ------

void* accept_peer_connections(void* arg){
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    struct sockaddr_in clientaddrIn;
    socklen_t sizeof_clientaddrIn = sizeof(struct sockaddr_in);
    fd_set recv_fds;
    int nfds;

    int n_connected_players, n_expected_players;
    n_connected_players = 0; n_expected_players = gameSession.n_players;

    for(int i = 0; i < gameSession.n_players; i++){
        gameSession.players[i]->client_fd = 0;
    }

    int select_ret = 0;
    while(1){
        pthread_mutex_lock(&gameMutex);
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

        if(!gameSession.game_in_progress){
            pthread_mutex_unlock(&gameMutex);
            break;
        }
        pthread_mutex_unlock(&gameMutex);

        FD_ZERO(&recv_fds);
        FD_SET(gameSession.p2p_fd, &recv_fds);
        nfds = gameSession.p2p_fd;

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        for(int i = 0; i < gameSession.n_players; i++){
            pthread_mutex_lock(clientMutexes + i);

            if(gameSession.players[i]->state != DISCONNECTED || gameSession.players[i]->state != FINISHED){
                if(gameSession.players[i]->client_fd > 0){
                    FD_SET(gameSession.players[i]->client_fd, &recv_fds);
                }

                if(nfds < gameSession.players[i]->client_fd){
                    nfds = gameSession.players[i]->client_fd;
                }
            }else{
                n_expected_players--;
            }

            pthread_mutex_unlock(clientMutexes + i);
        }

        select_ret = select(nfds + 1, &recv_fds, NULL, NULL, &timeout);

        if(select_ret <= 0){
            continue;
        }
        else if(FD_ISSET(gameSession.p2p_fd, &recv_fds) && n_connected_players < n_expected_players){
            int client_fd = accept(gameSession.p2p_fd, (struct sockaddr*) &clientaddrIn, &sizeof_clientaddrIn);

            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientaddrIn.sin_addr, ip, INET_ADDRSTRLEN);

            for(int i = 0; i < gameSession.n_players; i++){
                if(strcmp(gameSession.players[i]->ip, ip) == 0 && gameSession.players[i]->client_fd == 0){
                    pthread_mutex_lock(clientMutexes + i);
                    gameSession.players[i]->state = CONNECTED;
                    gameSession.players[i]->client_fd = client_fd;
                    pthread_mutex_unlock(clientMutexes + i);

                    n_connected_players++;
                    break;
                }
            }
        }else if(n_connected_players == n_expected_players){
            for(int i = 0; i < gameSession.n_players; i++){
                pthread_mutex_lock(clientMutexes + i);
                if(FD_ISSET(gameSession.players[i]->client_fd, &recv_fds)){
                    msg recv_client_msg = recv_msg(gameSession.players[i]->client_fd);

                    if(recv_client_msg.msg_type == INVALID){
                        gameSession.players[i]->state = DISCONNECTED;
                        if(gameSession.players[i]->client_fd > 0){ close(gameSession.players[i]->client_fd);} gameSession.players[i]->client_fd = 0;
                        if(gameSession.players[i]->server_fd > 0){ close(gameSession.players[i]->server_fd);} gameSession.players[i]->server_fd = 0;
                    }else{
                        switch(recv_client_msg.msg_type){
                            case FINISHED_GAME: gameSession.players[i]->state = FINISHED;
			                        if(gameSession.players[i]->client_fd > 0){ close(gameSession.players[i]->client_fd);} gameSession.players[i]->client_fd = 0;
                       				if(gameSession.players[i]->server_fd > 0){ close(gameSession.players[i]->server_fd);} gameSession.players[i]->server_fd = 0;
                                                break;
                            case LINES_CLEARED: pthread_mutex_lock(&gameMutex);
                                                gameSession.n_lines_to_add += strtol(recv_client_msg.msg, NULL, 10);
                                                pthread_mutex_unlock(&gameMutex);
                                                break;

                        }
                    }
                }
                pthread_mutex_unlock(clientMutexes + i);
            }
        }

        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    }

    pthread_exit(NULL);
}

void* service_peer_connections(void* arg){
    for(int i = 0; i < gameSession.n_players; i++){
        int client_server_fd = client_connect(gameSession.players[i]->ip, gameSession.players[i]->port);
        if(client_server_fd < 0){
            pthread_mutex_lock(clientMutexes + i);
            gameSession.players[i]->state = DISCONNECTED;
            if(gameSession.players[i]->client_fd > 0){ close(gameSession.players[i]->client_fd);} gameSession.players[i]->client_fd = 0;
            if(gameSession.players[i]->server_fd > 0){ close(gameSession.players[i]->server_fd);} gameSession.players[i]->server_fd = 0;
            pthread_mutex_unlock(clientMutexes + i);
        }
        else{
            pthread_mutex_lock(clientMutexes + i);
            gameSession.players[i]->server_fd = client_server_fd;
            pthread_mutex_unlock(clientMutexes + i);
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

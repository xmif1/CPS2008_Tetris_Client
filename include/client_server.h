#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#ifndef CPS2008_TETRIS_CLIENT_CLIENT_H
#define CPS2008_TETRIS_CLIENT_CLIENT_H

// NETWORKING CONFIG
#define PORT 8080
#define SDOMAIN AF_INET // or AF_INET6, correspondingly using netinet/in.h
#define IP_LOCALHOST "127.0.0.1"
#define TYPE SOCK_STREAM
#define MSG_SIZE 512
#define MSG_BUFFER_SIZE 20

// STRUCTS
typedef struct{
    int msg_type;
    char msg[MSG_SIZE];
}msg;

// FUNC DEFNS
int client_init();
int send_msg(msg send_msg, int socket_fd);
void enqueue_msg(int socket_fd);
msg dequeue_msg();

// GLOBALS
msg recv_msgs[MSG_BUFFER_SIZE];
pthread_mutex_t recv_msgs_mutex = PTHREAD_MUTEX_INITIALIZER;
int n_recv_msgs = 0;
enum MsgType {CHAT = 0, EMPTY = -1};

#endif //CPS2008_TETRIS_CLIENT_CLIENT_H

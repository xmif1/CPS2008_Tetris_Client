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
#define IP_LOCALHOST "192.168.68.127"   // "127.0.0.1"
#define TYPE SOCK_STREAM
#define MSG_LEN_DIGITS 4
#define HEADER_SIZE (MSG_LEN_DIGITS + 6) // '<msg_len>::<msg_type>::\0' where msg_len is of size MSG_LEN_DIGITS chars and msg_type is 1 char
#define MSG_BUFFER_SIZE 20

// STRUCTS
typedef struct{
    int msg_type;
    char* msg;
}msg;

// FUNC DEFNS
int client_init();
int send_msg(msg send_msg, int socket_fd);
int enqueue_msg(int socket_fd);
msg dequeue_chat_msg();
void handle_chat_msg(msg recv_msg);
void mrerror(char* err_msg);
void smrerror(char* err_msg);
void red();
void yellow();
void reset();

// GLOBALS
msg recv_chat_msgs[MSG_BUFFER_SIZE];
pthread_mutex_t threadMutex = PTHREAD_MUTEX_INITIALIZER;
int n_chat_msgs = 0;
enum MsgType {CHAT = 0, SCORE_UPDATE = 1, FINISHED_GAME = 2};

#endif //CPS2008_TETRIS_CLIENT_CLIENT_H

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
#define BUFFER_SIZE 1024
#define MSG_BUFFER_SIZE 20

// FUNC DEFNS
int client_init();
int send_msg(char* msg, int socket_fd);
void enqueue_msgs(int socket_fd);
char* dequeue_msgs();

// GLOBALS
char recv_msgs[MSG_BUFFER_SIZE][BUFFER_SIZE];
pthread_mutex_t recv_msgs_mutex = PTHREAD_MUTEX_INITIALIZER;
int n_recv_msgs = 0;

#endif //CPS2008_TETRIS_CLIENT_CLIENT_H

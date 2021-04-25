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
#define IP_LOCALHOST "10.62.14.54"  //"127.0.0.1" //"192.168.68.127"
#define TYPE SOCK_STREAM
#define MSG_LEN_DIGITS 4
#define HEADER_SIZE (MSG_LEN_DIGITS + 6) // '<msg_len>::<msg_type>::\0' where msg_len is of size MSG_LEN_DIGITS chars and msg_type is 1 char
#define MSG_BUFFER_SIZE 20

// GAME SESSION CONFIGS
#define N_SESSION_PLAYERS 8

// STRUCTS
typedef struct{
    char ip[INET_ADDRSTRLEN];
    int port;
    int client_fd;
    int server_fd;
    int state;
}ingame_client;

typedef struct{
    ingame_client* players[N_SESSION_PLAYERS];
    int p2p_fd;
    int score;
    int game_in_progress;
    int n_lines_to_add;
    int game_type;
    int n_players;
    int n_baselines;
    int n_winlines;
    int time;
}game_session;

typedef struct{
    int msg_type;
    char* msg;
}msg;

// FUNC DEFNS
int end_game();
int get_score();
int client_init();
int client_connect();
int send_msg(msg sendMsg, int socket_fd);
msg dequeue_chat_msg();
msg recv_msg(int socket_fd);
msg enqueue_server_msg(int socket_fd);
void* accept_peer_connections(void* arg);
void* service_peer_connections(void* arg);
void signalGameTermination();
void handle_chat_msg(msg recvMsg);
void handle_new_game_msg(msg recvMsg);
void red();
void reset();
void yellow();
void mrerror(char* err_msg);
void smrerror(char* err_msg);

// GLOBALS
msg recv_chat_msgs[MSG_BUFFER_SIZE];
int n_chat_msgs = 0;
pthread_mutex_t threadMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t gameMutex;
pthread_mutex_t clientMutexes[N_SESSION_PLAYERS];

int server_fd;
game_session gameSession;

enum MsgType {INVALID = -2, EMPTY = -1, CHAT = 0, SCORE_UPDATE = 1, NEW_GAME = 2, FINISHED_GAME = 3, P2P_READY = 4,
              CLIENTS_CONNECTED = 5, START_GAME = 6, LINES_CLEARED = 7};
enum GameType {RISING_TIDE = 0, FAST_TRACK = 1, BOOMER = 2};
enum State {WAITING = 0, CONNECTED = 1, FINISHED = 2, DISCONNECTED = 3};

#endif //CPS2008_TETRIS_CLIENT_CLIENT_H

// PROJETO SO 2016/2017
//
// João Pedro Costa Ferreiro 2014197760 || Guilherme Cardoso Gomes da Silva 2014226354
#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <semaphore.h>

// Produce debug information
#define	DEBUG	1

// Header of HTTP reply to client 
#define	SERVER_STRING	"Server: simpleserver/0.1.0\r\n"
#define	HEADER_1	"HTTP/1.0 200 OK\r\n"
#define	HEADER_2	"Content-Type: text/html\r\n\r\n"

#define	GET_EXPR	"GET /"
#define	SIZE_BUF	1024

#define PIPE_NAME	"config_pipe"


typedef struct{
	int socket;
	int is_static;
	int sche_type;
	char req_buf[SIZE_BUF];
}cell;

typedef struct{
	int write_pos;
	int read_pos;
	cell cell[SIZE_BUF];
}buffer;

typedef struct{
	int change_type;
	int change_type_sec;
	char allowed_files[SIZE_BUF][20];
}config_change;

typedef struct{
	int static_zip;
	char name[20];
	double time;
	char date[64];
	int stat_count;
}statistic;

typedef struct{
	int total_static;
	int total_zip;
	double avg_static;
	double avg_zip;
}stat_data;


void *configs();
void *worker();
void *scheduler_thread();
void init();
int load_config();
int statistics();
int fireup(int port);
void identify(int socket);
void get_request();
int read_line(int socket, int n);
void send_header(int socket);
void send_page(int socket);
void execute_script(int socket);
void not_found(int socket);
void cleanup(int);
void stat_handler(int);

char buf[SIZE_BUF], buf_tmp[SIZE_BUF];
char *allowed[20];
int SERVER_PORT,MAX_THREADS,CURRENT_SCHE_TYPE, shmid, socket_conn, stat_process;
int kill_workers = 0, sche_ok = 0, request_count = 0,processed_stats = 0;
long main_pid, stat_pid;
pthread_t config_thread, scheduler;
pthread_t *my_thread;
sem_t stat_semaphore;
statistic *to_send;
stat_data information;
buffer buff;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
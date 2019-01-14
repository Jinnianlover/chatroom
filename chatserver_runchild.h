#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define USER_LIMIT 5
#define BUFFER_SIZE 1024
#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024
#define PROCESS_LIMIT 65535

/*客户端数据*/
struct client_data
{
	sockaddr_in address; //客户端地址
	int connfd;          //连接文件描述符
	pid_t pid;           //处理连接的子进程的pid 
	int pipefd[2];       //于父进程通信的管道
};

bool stop_child = false;           //子进程状态控制
int sig_pipefd[2];                 //用于信号的传输
const char* shm_name = "/my_shm";
int epollfd;
int listenfd;
int shmfd;
char* share_mem = nullptr;
client_data* users = nullptr;
int* sub_process = nullptr;
int user_count = 0;
int setnonblocking(int fd);        //设置非阻塞函数
void addfd(int epollfd, int fd);   //将fd的读写注册到epollfd的内核事件表中
void sig_handler(int sig);         
void addsig(int sig, void(*handler)(int), bool restart = true);
void del_resource();
void child_term_handler(int sig);
/*count_id为客户编号，users指向客户数组，通过count_id为索引，
	share_mem共享内存起始地址*/
int run_child(int count_id, client_data* users, char* share_mem);

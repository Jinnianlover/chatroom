#include "chatserver_runchild.h"

int main(int argc, char* argv[])
{
	if (argc <= 2)
	{
		printf("usage: %s ip_address port_number\n");
		return -1;
	}
	const char* ip = argv[1];
	int port = atoi(argv[2]);

	int ret = 0;
	struct sockaddr_in address;
	bzero(&address, sizeof(address));
	address.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &address.sin_addr);
	address.sin_port = htons(port);
	
	listenfd = socket(PF_INET, SOCK_STREAM, 0);
	assert(listenfd >= 0);
	ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
	assert(ret != -1);
	ret = listen(listenfd, 5);
	assert(ret != -1);
	user_count = 0;
	users = new client_data[USER_LIMIT + 1];
	sub_process = new int [PROCESS_LIMIT];
	for (int i = 0; i < PROCESS_LIMIT; i++)
		sub_process[i] = -1;
	epoll_event events[MAX_EVENT_NUMBER];
	epollfd = epoll_create(5);
	assert(epollfd != -1);
	addfd(epollfd, listenfd);
	
	ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);   //建立匿名套接字双工通信管道
	assert(ret != -1);
	setnonblocking(sig_pipefd[1]);
	addfd(epollfd, sig_pipefd[0]);

	addsig(SIG_CHLD, sig_handler);
	addsig(SIG_TERM, sig_handler);
	addsig(SIG_INT, sig_handler);
	addsig(SIG_PIPE, SIG_IGN);
	bool stop_server = false;
	bool terminate = false;

	shmfd = shm_open(shm_name, O_CREATE | O_RDWR, 0666);
	assert(shmfd != -1);
	ret = ftruncate(shmfd, USER_LINIT * BUFFER_SIZE);
	assert(ret != -1);
	
	share_mem = (char*)mmap(NULL, USER_LIMIT * BUFFER_SIZE, PORT_READ | PORT_WRITE, MAP_SHARED, shmfd, 0);
	assert(share_mem != MAP_FAILED);
	close(shmfd);

	while (!stop_server)
	{
		int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
		if ((number < 0) && (errno != EINTR))
		{
			perror("epoll failure");
			break;
		}
		for (int i = 0; i < number; i++)
		{
			int sockfd = events[i].data.fd;
			/*当有新的连接就绪，处理新的连接*/
			if (sockfd == listenfd)
			{
				struct sockaddr_in client_address;
				socklen_t client_addrlenth = sizeof(client_address);
				int connfd = accept(sockfd, (struct sockaddr*)&client_address, &client_addrlenth);
				if (connfd < 0)
				{
					perror("accept");
					continue;
				}
				if (user_count >= USER_LIMIT)
				{
					const char* info = "have 5 user\n";
					printf("%s",info);
					send(connfd, info, strlen(info), 0);
					close(connfd);
					continue;
				}

				users[user_count].address = client_address;
				users[user_count].connfd = connfd;
				ret = socketpair(PF_UNIX, SOCK_STEAM, 0, users[count].pipefd_tochild); //建立匿名套接字双工通信管道，
				                                                                       //不用建立两个单向
				assert(ret != -1);

				pid_t pid = fork();  //创建子进程
				if (pid < 0)
				{
					close(connfd);
					continue;
				}
				else if (pid = 0)
				{	
					close(users[count].pipefd[1]);    //子进程关闭一端管道
					/*释放复制而来到资源*/
					close(epollfd);              
					close(listenfd);
					close(sig_pipefd[0]);
					close(sig_pipefd[1]);
					run_child(user_count, users, share_mem); //运行子进程事件处理函数体
					munmap((void*)share_mem, USER_LIMIT * BUFFER_SIZE);  //释放复制而来的资源
					exit(0);
				}
				else if (pid > 0)
				{
					close(users[user_count].pipefd[0]);
					close(connfd);
					addfd(epollfd, users[user_count].pipefd[1]);
					users[user_count].pid = pid;
					sub_process[pid] = user_count;  //以用进程号为索引，建立用户编号关系
					user_count++;
				}
			}
			/*处理信号*/
			if ((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN))
			{
				int sig;
				char signal[1024];
				ret = recv(sockfd, signal, sizeof(signal), 0);	
				if (ret <= 0)
				{
					continue;
				}
				else if (ret > 0)
				{
					/*每个信号1字节，安字节接收信号*/
					for (int i = 0; i < ret; i++)
					{
						switch (signal[i])
						{
							/*当某个客户端断开连接，即某子进程结束退出，会给父进程发送CHLD信号*/
							case SIGCHLD:
							{
								pid_t pid;
								int stat;
								while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
								{
									int del_user = sub_process[pid];
									sub_process[pid] = -1;
									/*通过用户编号关闭管道*/
									epoll_ctl(epollfd, EPOLL_CTL_DEL, users.[del_user].pipefd[1], 0);
									close(users.[del_user].pipefd[1]);
									/*让最后一位客户端覆盖原来断开的客户端，保证user_count小于USER_LIMIT*/
									users[del_user] = users[--user_count];
									sub_process[users[del_user].pid] = del_user;
								}
								if (user_count == 0)
								{
									stop_server = true;
								}
								break;
							}
							/*结束服务器信号*/
							case SIGTERM:
							case SIGINT:
							{
								printf("kill all the child process now\n");
								for (int i = 0; i < user_count; i++)
									kill(users[i].pid, SIGTERM);
								stop_server = true;
								break;
							}
							default: break;
						}
					}
					}
				}
				/*此处处理子进程写入数据，无法定位哪一个进程，所以在判断信号后判断*/
			else if (events[i].events & EPOLLIN)
			{
				int child;
				ret = recv(sockfd, &child, sizeof(child), 0);
				if (ret <= 0)
				{
					continue;
				}
				else 
				{
					for (int i = 0; i < user_count; i++)
					{
						if (i != child)
						{
							send(users[i].pipefd[1], (char*)&child, sizeof(child), 0);
						}
					}
				}
			}
		}
	}
	del_resource();
	return 0;
}

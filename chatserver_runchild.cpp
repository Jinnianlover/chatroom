#include "chatserver2.h"

int setnonblock(int fd)
{
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

void addfd(int epollfd, int fd)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET;             //ET模式
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);//注册fd事件
	setnonblock(fd);                              //设置为非阻塞
}

void sig_handler(int sig)
{
	int save_errno = errno;
	int msg = sig;
	send(sig_pipefd[1], (char*)&msg, 1, 0);
	errno = save_errno;
}

void addsig(int sig, void(*handler)(int), bool restart = true)
{
	struct sigaction sa;
	memset(&sa, '\0', sizeof(sa));
	sa.sa_handler - handler;
	if (restart)
	{
		sa.sa_flags |= SA_RESTART;
	}
	sidfillset(&sa.sa_mask);
	assert(sigaction(sig, &sa, NULL) != -1);
}

void child_term_handler(int sig)
{
	stop_child = true;
}

int run_child(int count_id, client_data* user, char* share_mem);
{
	epoll_event events[MAX_EVENT_NUMBER];
	int child_epollfd = epoll_create(5);
	assert(child_epollfd != -1);
	int connfd = users[count_id].connfd;
	addfd(child_epollfd, connfd);
	int pipefd = users[count_id].pipefd[0];
	addfd(child_epoll, pipefd);
	int ret;

	addsig(SIGTERM, child_term_handler, false);

	while (!stop_child)
	{
		int number = epoll_wait(child_epollfd, events, MAX_EVENT_NUMBER, -1);
		if ((number < 0) && (errno != EINTR))
		{
			perror("epoll");
			break;
		}
		for (int i = 0; i < number; i++)
		{
			int sockfd = events[i].data.fd;
			/*子进程对应客户端有数据到达*/
			if ((sockfd == connfd) && (events[i].events & EPOLLIN))
			{
				memset(share_mem + count_id * BUFFER_SIZE, '\0', BUFFER_SIZE);
				ret = recv(sockfd , share_mem + count_id * BUFFER_SIZE, BUFFER_SIZE - 1, 0);
				if (ret < 0)
				{
					if (errno != Eagain)
					{
						stop_child = true;
					}
				}
				else if (ret = 0)
				{
					stop_child = true;
				}
				else
				{
					/*通知父进程*/
					send(pipefd, (char*)&count_id, sizeof(count_id), 0);
				}

			}
			else if ((sockfd == pipefd) && (events[i].events & EPOLLIN))
			{
				int client = 0;
				ret = recv(sockfd, (char*)&client, sizeof(client), 0);
				if (ret < 0)
				{
					if (errno != EAGAIN)
					{
						stop_child = true;
					}
				}
				else if (ret == 0)
				{
					stop_child = true;
				}
				else
				{
					send(connfd, share_mem + client * BUFFER-SIZ, BUFFER_SIZE, 0);
				}
			}
			else
				continue;
		}
	}
	close(connfd);
	close(pipefd);
	close(child_epollfd);
}

void del_resource()
{
	close(sig_pipefd[0]);
	close(sig_pipefd[1]);
	close(epollfd);
	close(listenfd);
	shm_unlink(shm_name);
	delete [] users;
	delete [] sub_process;
}

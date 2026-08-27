#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>

#include "configs.h"

#define MAX_EVENTS 128
#define FD_CHAR_SIZE 24

typedef struct ep_data_t{
	struct config_t *config;
	int fd;
} ep_data_t;

int socket_epoll_add(config_t *config, int efd){ // Fatal exits dont worry about mem or fds
	address_t *current = config->addr;
	while(current){
		ep_data_t *data = calloc(1, sizeof(ep_data_t));
		if(!data){
			return -1;
		}
		data->config = config;
		if(current->family == AF_INET){
			data->fd = socket(AF_INET, SOCK_STREAM, 0);
		}
		else{
			data->fd = socket(AF_INET6, SOCK_STREAM, 0);
		}

		if(data->fd == -1){
			printf("Error while creating socket\n");
			return -1;
		}

		int optval = 1;
		if(setsockopt(data->fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) == -1){
			printf("Error setting socket SO_REUSEPORT\n");
			return -1;
		}

		if(fcntl(data->fd, F_SETFL, fcntl(data->fd, F_GETFL, 0) | O_NONBLOCK) == -1){
			printf("Error setting O_NONBLOCK on socket\n");
			return -1;
		}

		if(fcntl(data->fd, F_SETFL, fcntl(data->fd, F_GETFL, 0) | O_CLOEXEC) == -1){
			printf("Error setting O_CLOEXEC on socket\n");
			return -1;
		}

		if(bind(data->fd, (struct sockaddr *)&current->addr, current->addr_len) == -1){
			printf("Error binding socket\n");
			return -1;
		}

		if(listen(data->fd, SOMAXCONN) == -1){
			printf("Error setting socket to listen mode\n");
			return -1;
		}

		struct epoll_event event = {0};
		event.events |= EPOLLIN;
		event.data.ptr = data;

		if(epoll_ctl(efd, EPOLL_CTL_ADD, data->fd, &event) == -1){
			printf("Error adding file descriptor to epoll - %s\n", strerror(errno));
			return -1;
		}

		current = current->next;
	}

	return 0;
}

int main(){
	config_t *configs = NULL;
	int num_configs = parse_configs(DEFAULT_CONFIG_FILE, &configs);
	if(num_configs == -1){
		printf("Encountered fatal error while parsing config file\n");
		return 1;
	}
	if(config_nerrors()){
		printf("Encountered %d errors while parsing configs\n", config_nerrors());
		return 1;
	}
	if(num_configs == 0){
		printf("Zero services loaded, exiting...\n");
		return 0;
	}

	int efd = epoll_create(1);
	if(efd == -1){
		printf("Fatal error starting epoll - %s\n", strerror(errno));
		return 1;
	}

	if(fcntl(efd, F_SETFL, fcntl(efd, F_GETFL, 0) | O_CLOEXEC) == -1){
		printf("Error setting O_CLOEXEC on epoll instance\n");
		return -1;
	}

	for(config_t *current = configs; current; current = current->next){
		if(socket_epoll_add(current, efd) == -1){
			printf("Socket creation error exiting...\n");
			return 1;
		}
	}

	struct epoll_event events[MAX_EVENTS];

	signal(SIGCHILD, SIG_IGN);

	while(1){
		int ready_fds = epoll_wait(efd, events, MAX_EVENTS, -1);
		if(ready_fds == -1){
			printf("Epoll wait exited with error - %s", strerror(errno));
			continue;
		}
		for(int i = 0; i < ready_fds; i++){
			ep_data_t *data = events[i].data.ptr;
			int cfd = accept(data->fd, NULL, NULL);
			if(cfd == -1){
				printf("Accept exited with error - %s\n", strerror(errno));
				continue;
			}

			pid_t proc = fork();
			if(proc == -1){
				printf("Fork exitted with error - %s\n", strerror(errno));
				close(cfd);
				continue;
			}
			else if(proc != 0){
				close(cfd);
				continue;
			}
			else{
				char fd_arg[FD_CHAR_SIZE] = {0};
				snprintf(fd_arg, sizeof(fd_arg), "%d", cfd);
				if(execl(data->config->filepath, data->config->name, fd_arg, (char *)NULL) == -1){
					printf("Exec exitted with error - %s\n", strerror(errno));
					_exit(1);
				}
				printf("Service Started: %s\n", data->config->name);
			}
		}
	}

	return 0;
}

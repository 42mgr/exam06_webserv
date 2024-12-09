#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

typedef struct s_client {
	int id;
	char msg[300000];
} t_client;

fd_set write_set, read_set, current_set;
int gid = 0, maxfd = 0;
char write_buffer[600000], recv_buffer[600000];
t_client clients[1024];

socklen_t len;
struct sockaddr_in serveraddr;

void err(char *msg) {
	if (msg)
		write(2, msg, strlen(msg));
	else
		write(2, "Fatal error", 11);
	write(2, "\n", 1);
	exit(1);
}

void send_to_all(int except) {
	for (int fd = 0; fd <= maxfd; fd++) {
		if (FD_ISSET(fd, &write_set) && fd != except) {
			if(send(fd, &write_buffer, strlen(write_buffer), 0) == -1)
				err(NULL);
		}
	}
	bzero(write_buffer, sizeof(write_buffer));
}

int main(int ac, char **av) {
	if (ac != 2)
		err("Wrong number of arguments");

	int serverfd = socket(AF_INET, SOCK_STREAM, 0);
	if (serverfd == -1)
		err(NULL);
	maxfd = serverfd;

	bzero(clients, sizeof(clients));
	bzero(&serveraddr, sizeof(serveraddr));

	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(atoi(av[1]));
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	FD_ZERO(&current_set);
	FD_SET(serverfd, &current_set);

	if (bind(serverfd, (const struct sockaddr*)&serveraddr, sizeof(serveraddr)) == -1 || listen(serverfd, 100) == -1)
		err(NULL);

	while(1) {
		write_set = read_set = current_set;

		if (select(maxfd + 1, &read_set, &write_set, 0, 0) == -1)
			continue;

		for (int fd = 0; fd <= maxfd; fd++) {
			if (FD_ISSET(fd, &read_set)) {
				if (fd == serverfd) {

					// accept
					int clientfd = accept(serverfd, (struct sockaddr*)&serveraddr, &len);
					if (clientfd == -1)
						err(NULL);
					if (clientfd > maxfd)
						maxfd = clientfd;
					
					clients[clientfd].id = gid++;

					FD_SET(clientfd, &current_set);
					sprintf(write_buffer, "server: client %d connected\n", clients[clientfd].id);
					send_to_all(clientfd);
	
				} else {
					int ret = recv(fd, &recv_buffer, sizeof(recv_buffer), 0);

					if (ret <= 0) {
						// disconnect

						FD_CLR(fd, &current_set);
						sprintf(write_buffer, "server: client %d just left\n", clients[fd].id);
						send_to_all(fd);

						close(fd);
						bzero(clients[fd].msg, sizeof(clients[fd].msg));
				
					} else {
						// read
						for (int i = 0, j = strlen(clients[fd].msg); i < ret; i++, j++) {
							clients[fd].msg[j] = recv_buffer[i];
							if (clients[fd].msg[j] == '\n') {
								clients[fd].msg[j] = '\0';
								sprintf(write_buffer, "client %d: %s\n", clients[fd].id, clients[fd].msg);
								send_to_all(fd);
								bzero(clients[fd].msg, sizeof(clients[fd].msg));
								j = -1;
							}
						}
					}
				}
			}
		}
	}
}


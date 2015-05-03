/* clang -W -Wall -Wextra -g -o socket_activation socket_activation.c `pkg-config --cflags --libs libsystemd` */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <systemd/sd-daemon.h>

int main(void) {
	int num = sd_listen_fds(0);
	if (num != 1) {
		fprintf(stderr, "No or too many file descriptors received: %d\n", num);
		exit(1);
	}

	int fd = SD_LISTEN_FDS_START + 0;
	printf("%d\n\n", fd);

	struct sockaddr_in client;
	socklen_t len = sizeof(client);
	int c = accept(fd, (struct sockaddr*) &client, &len);

	char buf[1000];
	while (1) {
		int bytes = read(c, buf, 1000);
		if (bytes <= 0) {
			printf("error=%d\n", bytes);
			break;
		}
		buf[bytes] = '\0';
		printf("%s", buf);
	}

	printf("DONE\n");
}


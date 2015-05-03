/* clang -W -Wall -Wextra -g -o socket_activation_ev socket_activation_ev.c `pkg-config --cflags --libs libsystemd libevent` */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <systemd/sd-daemon.h>

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

struct event_base *base = NULL;
int active = 0;

static void echo_read_cb(struct bufferevent *bev, void *ctx) {
	/* This callback is invoked when there is data to read on bev. */
	struct evbuffer *input = bufferevent_get_input(bev);

	char buf[1000];
	int bytes = evbuffer_remove(input, buf, 1000);
	if (bytes <= 0) {
		printf("error=%d\n", bytes);
		return;
	}
	buf[bytes] = '\0';
	printf("%s\n", buf);
}

static void echo_event_cb(struct bufferevent *bev, short events, void *ctx) {
	if (events & BEV_EVENT_ERROR)
		perror("Error from bufferevent");
	if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
		bufferevent_free(bev);
		active--;
	}
}

static
void cb_new_connection(struct evconnlistener *listener, evutil_socket_t fd,
		struct sockaddr *client, int socklen, void *ctx) {
	/* We got a new connection! Set up a bufferevent for it. */
	struct event_base *base = evconnlistener_get_base(listener);
	struct bufferevent *bev = bufferevent_socket_new(base, fd,
			BEV_OPT_CLOSE_ON_FREE);

	bufferevent_setcb(bev, echo_read_cb, NULL, echo_event_cb, NULL);

	bufferevent_enable(bev, EV_READ | EV_WRITE);

	active++;
	printf("cb_new_connection\n");
}

void cb_tim(evutil_socket_t fd, short what, void *ctx) {
	if (active <= 0) {
		event_base_loopbreak(base);
	}
}

struct evconnlistener *init_socket_activated(void) {
	evutil_socket_t fd = SD_LISTEN_FDS_START + 0;
	evutil_make_socket_nonblocking(fd);

	return evconnlistener_new(base, cb_new_connection,
	NULL, 0, 0 /* Set to 0 if the socket is already listening*/, fd);
}

struct evconnlistener *init_socket_standalone(int port) {
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(0);
	sin.sin_port = htons(port);

	return evconnlistener_new_bind(base, cb_new_connection, NULL,
	LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1, (struct sockaddr*) &sin,
			sizeof(sin));
}

int main(int argc, char **argv) {

	base = event_base_new();
	struct evconnlistener *listener = NULL;

	if (sd_listen_fds(0) == 1) {
		listener = init_socket_activated();
	} else if (argc == 2) {
		int port = atoi(argv[1]);
		listener = init_socket_standalone(port);
	} else {
		exit(EXIT_FAILURE);
	}

	struct event *tim = event_new(base, 0, EV_TIMEOUT | EV_PERSIST, cb_tim,
	NULL);
	struct timeval tv = { 1, 0 };
	event_add(tim, &tv);

	event_base_dispatch(base);

	event_free(tim);
	evconnlistener_free(listener);
	event_base_free(base);

	printf("DONE\n");
	return 0;
}


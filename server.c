#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <math.h>
#include <netdb.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_CLIENTS FD_SETSIZE
#define MAX_NICK 12
#define RBUF 4096

typedef struct {
    int fd;
    int registered;
    char nick[MAX_NICK + 1];
    char buf[RBUF];
    size_t bufpos;
} client_t;

static client_t clients[MAX_CLIENTS];
static int listenfd;

static int setup_listener(const char *host, const char *port) {
  struct addrinfo hints = {0}, *res, *rp;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if (getaddrinfo(host, port, &hints, &res) != 0)
    return -1;

  int s = -1;
  int opt = 1;

  for (rp = res; rp; rp = rp->ai_next) {
    s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (s == -1)
      continue;

    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(s, rp->ai_addr, rp->ai_addrlen) == 0)
      break;

    close(s);
    s = -1;
  }

  freeaddrinfo(res);

  if (s == -1)
    return -1;
  if (listen(s, 16) == -1) {
    close(s);
    return -1;
  }
  return s;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s host:port\n", argv[0]);
    fflush(stderr);
    return EXIT_FAILURE;
  }

  char *hp = strdup(argv[1]);
  if (!hp)
    return EXIT_FAILURE;

  char *colon = strrchr(hp, ':');
  if (!colon) {
    free(hp);
    return EXIT_FAILURE;
  }

  *colon = '\0';
  listenfd = setup_listener(hp, colon + 1);

  printf("Server listening on %s:%s\n", hp, colon + 1);
  fflush(stdout);
  free(hp);
  for (int i = 0; i < MAX_CLIENTS; i++)
    clients[i].fd = -1;

  fd_set fds;

  while (1) {
    FD_ZERO(&fds);
    FD_SET(listenfd, &fds);
    int maxfd = listenfd;

    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (clients[i].fd != -1) {
        FD_SET(clients[i].fd, &fds);
        if (clients[i].fd > maxfd)
          maxfd = clients[i].fd;
      }
    }

    select(maxfd + 1, &fds, NULL, NULL, NULL);
  }
}
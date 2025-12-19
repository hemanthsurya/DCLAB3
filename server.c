#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <regex.h>

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

static void send_to_client(int fd, const char *msg) {
  ssize_t n = write(fd, msg, strlen(msg));
  if (n < 0) {
    perror("write");
    fflush(stderr);
  }
}

static void broadcast(const char *msg) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].fd != -1 && clients[i].registered) {
      send_to_client(clients[i].fd, msg);
    }
  }
}

static void remove_client(int i) {
  close(clients[i].fd);
  clients[i].fd = -1;
  clients[i].registered = 0;
  clients[i].bufpos = 0;
  clients[i].nick[0] = '\0';
}

static int valid_nick(const char *nick) {
  regex_t re;
  if (!nick || strlen(nick) == 0 || strlen(nick) > MAX_NICK)
    return 0;

  regcomp(&re, "^[A-Za-z0-9_]{1,12}$", REG_EXTENDED | REG_NOSUB);
  int ok = (regexec(&re, nick, 0, NULL, 0) == 0);
  regfree(&re);
  return ok;
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

    if (FD_ISSET(listenfd, &fds)) {
      struct sockaddr_storage ss;
      socklen_t slen = sizeof(ss);
      int cfd = accept(listenfd, (struct sockaddr *)&ss, &slen);

      if (cfd >= 0) {
        char addr[INET6_ADDRSTRLEN];
        char port[6];

        getnameinfo((struct sockaddr *)&ss, slen, addr, sizeof(addr), port,
                    sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);

        printf("Client connected from %s:%s\n", addr, port);
        printf("Server protocol: HELLO 1\n");
        fflush(stdout);

        for (int i = 0; i < MAX_CLIENTS; i++) {
          if (clients[i].fd == -1) {
            clients[i].fd = cfd;
            clients[i].bufpos = 0;
            clients[i].registered = 0;
            send_to_client(cfd, "Hello 1\n");
            break;
          }
        }
      }
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (clients[i].fd == -1)
        continue;

      int fd = clients[i].fd;
      if (!FD_ISSET(fd, &fds))
        continue;

      ssize_t r = read(fd, clients[i].buf + clients[i].bufpos,
                       RBUF - clients[i].bufpos - 1);

      if (r <= 0) {
        remove_client(i);
        continue;
      }

      clients[i].bufpos += r;
      clients[i].buf[clients[i].bufpos] = '\0';

      char *start = clients[i].buf;
      char *end = start + clients[i].bufpos;

      while (1) {
        char *nl = strchr(start, '\n');
        if (!nl)
          break;

        *nl = '\0';

        if (strncmp(start, "NICK ", 5) == 0) {
          char *nick = start + 5;
          if (valid_nick(nick)) {
            strcpy(clients[i].nick, nick);
            clients[i].registered = 1;
            send_to_client(fd, "OK\n");
            printf("Name is allowed\n");
            fflush(stdout);
          } else {
            send_to_client(fd, "ERR invalid nick\n");
          }
        } else if (strncmp(start, "MSG ", 4) == 0 && clients[i].registered) {
          char out[512];
          snprintf(out, sizeof(out), "MSG %s %s\n", clients[i].nick, start + 4);
          broadcast(out);
        }

        start = nl + 1;
      }

      memmove(clients[i].buf, start, end - start);
      clients[i].bufpos = end - start;
    }
  }
}
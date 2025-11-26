#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <netdb.h>
#include <poll.h>


int main(int argc, char *argv[]){
  if (argc != 2)
  {
    printf("Expected: <ip>:<port>\n");
  }
  
  char delim[] = ":";
  char *Desthost = strtok(argv[1], delim);
  char *Destport = strtok(NULL, delim);
  int port = atoi(Destport);
  if (!Desthost || !Destport) {
    printf("Wrong input arguments\n");
    return 1;
  }

  struct addrinfo hints, *res, *rp;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int err = getaddrinfo(Desthost, Destport, &hints, &res);
  if (err != 0) {
#ifdef DEBUG
    printf("getaddrinfo\n");
#endif
    return 1;
  }
  int listen_fd = -1;
  for (rp = res; rp; rp = rp->ai_next) {
    listen_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (listen_fd == -1)
      continue;
    int i = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));
    if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) == 0)
      break;
    close(listen_fd);
    listen_fd = -1;
  }
  freeaddrinfo(res);

  if (listen_fd == -1) {
#ifdef DEBUG
    printf("ERROR: BIND FAILED");
#endif
    return 1;
  }

  if (listen(listen_fd, 5) < 0) {
#ifdef DEBUG
    printf("ERROR: LISTEN FAILED");
#endif
    close(listen_fd);
    return 1;
  }

  printf("Server listening on %s:%d\n", Desthost, port);
 
  while (1) {
    struct sockaddr_storage client_addr;
    socklen_t client_len = sizeof(client_addr);
    int connection_fd =
        accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
    if (connection_fd < 0) {
      printf("Connection fd error\n");
      continue;
    }
  }
}

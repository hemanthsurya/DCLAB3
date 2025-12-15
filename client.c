#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

#define MAX_NICK 12
#define READBUF 4096
#define MAX_MSG 255

static struct termios orig_term;
static int sockfd = -1;
static char mynick[MAX_NICK + 1];

static void restore_terminal(void) {
  tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
}

static void setup_terminal(void) {
  tcgetattr(STDIN_FILENO, &orig_term);
  atexit(restore_terminal);

  struct termios t = orig_term;
  t.c_lflag &= ~(ECHO | ICANON);
  t.c_cc[VMIN] = 1;
  t.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

static void die(const char *msg) {
  perror(msg);
  fflush(stderr);
  restore_terminal();
  if (sockfd >= 0)
    close(sockfd);
  exit(EXIT_FAILURE);
}

static int validate_nick(const char *nick) {
  regex_t re;
  if (!nick || strlen(nick) == 0 || strlen(nick) > MAX_NICK)
    return 0;
  regcomp(&re, "^[A-Za-z0-9_]{1,12}$", REG_EXTENDED | REG_NOSUB);
  int ok = (regexec(&re, nick, 0, NULL, 0) == 0);
  regfree(&re);
  return ok;
}

static int connect_to_server(const char *host, const char *port) {
  struct addrinfo hints = {0}, *res, *rp;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(host, port, &hints, &res) != 0)
    return -1;

  int s = -1;
  for (rp = res; rp; rp = rp->ai_next) {
    s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (s == -1)
      continue;
    if (connect(s, rp->ai_addr, rp->ai_addrlen) == 0)
      break;
    close(s);
    s = -1;
  }
  freeaddrinfo(res);
  return s;
}

int main(int argc, char *argv[]) {

  if (argc == 2) {
    fprintf(
        stderr,
        "Too few arguments!\nExpected: <server-ip>:<server-port> <nickname>\n");
    fflush(stderr);
    return EXIT_FAILURE;
  }

  if (argc != 3) {
    fprintf(stderr, "Usage: %s host:port nick\n", argv[0]);
    fflush(stderr);
    return EXIT_FAILURE;
  }

  char *hp = strdup(argv[1]);
  char *colon = strrchr(hp, ':');
  if (!colon || !validate_nick(argv[2])) {
    fprintf(stderr, "Invalid arguments\n");
    fflush(stderr);
    free(hp);
    return EXIT_FAILURE;
  }

  *colon = '\0';
  char *host = hp;
  char *port = colon + 1;
  strncpy(mynick, argv[2], sizeof(mynick) - 1);

  sockfd = connect_to_server(host, port);
  if (sockfd < 0)
    die("connect");

  printf("Connected to %s:%s\n", host, port);
  fflush(stdout);

  free(hp);

  char buf[READBUF];
  ssize_t n = read(sockfd, buf, sizeof(buf) - 1);
  if (n <= 0)
    die("read");
  buf[n] = '\0';

  printf("Server protocol: %s", buf);
  fflush(stdout);

  printf("Protocol supported, sending nickname\n");
  fflush(stdout);

  char nickmsg[64];
  snprintf(nickmsg, sizeof(nickmsg), "NICK %s\n", mynick);
  if (write(sockfd, nickmsg, strlen(nickmsg)) < 0)
    die("write");

  n = read(sockfd, buf, sizeof(buf) - 1);
  if (n <= 0)
    die("read");
  buf[n] = '\0';

  if (strncmp(buf, "OK", 2) != 0) {
    fprintf(stderr, "%s", buf);
    fflush(stderr);
    return EXIT_FAILURE;
  }

  printf("Name accepted!\n");
  fflush(stdout);

  setup_terminal();

  /* ---- buffers ---- */
  char sockbuf[READBUF];
  size_t socklen = 0;

  char input[MAX_MSG + 1];
  size_t inpos = 0;

  fflush(stdout);

  while (1) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sockfd, &fds);
    FD_SET(STDIN_FILENO, &fds);

    if (select(sockfd + 1, &fds, NULL, NULL, NULL) < 0)
      die("select");

    /* ---- socket ---- */
    if (FD_ISSET(sockfd, &fds)) {
      ssize_t r =
          read(sockfd, sockbuf + socklen, sizeof(sockbuf) - socklen - 1);
      if (r <= 0)
        break;

      socklen += r;
      sockbuf[socklen] = '\0';

      char *start = sockbuf;
      char *end = sockbuf + socklen;

      while (1) {
        char *nl = strchr(start, '\n');
        if (!nl)
          break;
        *nl = '\0';

        if (strncmp(start, "MSG ", 4) == 0) {
          char *p = start + 4;
          char *sp = strchr(p, ' ');
          if (sp) {
            *sp = '\0';
            if (strcmp(p, mynick) != 0)
              printf("%s: %s\n", p, sp + 1);
          }
        }

        start = nl + 1;
      }

      memmove(sockbuf, start, end - start);
      socklen = end - start;

      printf("%.*s", (int)inpos, input);
      fflush(stdout);
    }
  }
}
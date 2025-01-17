#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PORT 8086
#define CLIENTI 20
#define MESSAGE_SIZE 1024

#define error(msg)      \
  {                     \
    perror(msg);        \
    exit(EXIT_FAILURE); \
  }

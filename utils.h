#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PORT 8086
#define MAX_CLIENTS 20
#define MESSAGE_SIZE 1024

#define error(msg)      \
  {                     \
    perror(msg);        \
    exit(EXIT_FAILURE); \
  }

typedef struct
{
  int socket;
  int id;
  char username[64];
  int to_close;

} Client;

Client clients[MAX_CLIENTS];
int connected_clients = 0;
int additional_clients = 0;
fd_set active_fds; // set of active file descriptors

char *extractRecipientUsername(char *message)
{
  int j = 0;
  while (message[j] != ':' && message[j] != '\0')
    j++;

  char *username = (char *)malloc(j + 1);
  if (username == NULL)
    error("Memory allocation error\n");

  memset(username, 0, j + 1);
  strncpy(username, message, j);
  strcpy(message, message + j + 1);

  return username;
}

int isNumericString(const char *str)
{
  while (*str)
  {
    if (!isdigit(*str))
      return 0;
    str++;
  }
  return 1;
}

int extractReplyId(char *message)
{
  if (strncmp(message, "<", 1) == 0)
  {
    const char *end_id = strchr(message + 1, '>');
    if (end_id != NULL)
    {
      char reply_id_str[20];
      memset(reply_id_str, 0, sizeof(reply_id_str));
      strncpy(reply_id_str, message + 1, end_id - (message + 1));

      if (isNumericString(reply_id_str))
      {
        strcpy(message, message + 2 + strlen(reply_id_str));
        int reply_id = atoi(reply_id_str);
        return reply_id;
      }
    }
  }
  return 0;
}

int isIdExisting(int id)
{
  for (int i = 0; i < connected_clients + additional_clients; i++)
  {
    if (clients[i].id == id)
    {
      return 1;
    }
  }
  return 0;
}

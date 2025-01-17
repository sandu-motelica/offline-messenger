#include "utils.h"

int main()
{
  int client_socket;
  char username[64], recipient_username[64], message[MESSAGE_SIZE] = {0};

  if (-1 == (client_socket = socket(AF_INET, SOCK_STREAM, 0)))
    error("[Client] Error creating socket\n");

  struct sockaddr_in server;
  bzero(&server, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(PORT);
  server.sin_addr.s_addr = htonl(INADDR_ANY);

  if (0 > connect(client_socket, (struct sockaddr *)&server, sizeof(server)))
    error("Error on connect()\n");
  printf("Enter your username: ");
  fflush(stdout);
  fgets(username, sizeof(username), stdin);
  username[strcspn(username, "\n")] = '\0';

  send(client_socket, username, strlen(username), 0);
  fd_set readfds;
  while (1)
  {
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    FD_SET(client_socket, &readfds);

    if (0 > select(client_socket + 1, &readfds, NULL, NULL, NULL))
      error("[Client] Error on select()\n");

    if (FD_ISSET(client_socket, &readfds))
    {
      memset(message, 0, sizeof(message));
      if (0 < recv(client_socket, message, sizeof(message), 0))
      {
        printf("%s", message);
        fflush(stdout);
      }
    }

    if (FD_ISSET(STDIN_FILENO, &readfds))
    {
      memset(message, 0, sizeof(message));
      fgets(message, sizeof(message), stdin);
      message[strcspn(message, "\n")] = '\0';

      if (strlen(message) > 0)
      {
        if (strcmp(message, "quit") == 0)
        {
          break;
        }
        send(client_socket, message, strlen(message), 0);
      }
    }
  }
  close(client_socket);
  return 0;
}

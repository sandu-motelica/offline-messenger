#include <ctype.h>
#include <netinet/in.h>
#include <pthread.h>
#include "utils.h"
#include "database.h"

void closeConnection(int client_index)
{
  printf("Client '%s' has disconnected\n", clients[client_index].username);
  close(clients[client_index].socket);
  FD_CLR(clients[client_index].socket, &active_fds);

  memset(clients[client_index].username, 0, sizeof(clients[client_index].username));
  clients[client_index].socket = 0;
  clients[client_index].id = 0;
  clients[client_index].to_close = 0;
  connected_clients--;
  additional_clients++;
}

int main()
{
  char buffer[MESSAGE_SIZE];
  int server_socket, client_socket, optval = 1;

  int response = sqlite3_open("mess.db", &db);
  if (response != SQLITE_OK)
  {
    sqlite3_close(db);
    error("Error opening/creating the database");
  }

  if (-1 == (server_socket = socket(AF_INET, SOCK_STREAM, 0)))
    error("[Server] Error creating socket\n");

  // Set socket option SO_REUSEADDR
  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

  struct sockaddr_in server, client;
  socklen_t len = sizeof(struct sockaddr_in);

  // Initialize the server address structure
  bzero(&server, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(PORT);
  server.sin_addr.s_addr = htonl(INADDR_ANY);

  if (-1 == bind(server_socket, (struct sockaddr *)&server, sizeof(struct sockaddr)))
    error("Error on bind()\n");

  if (-1 == listen(server_socket, MAX_CLIENTS))
    error("Error on listen()\n");

  printf("[Server] Listening on port %d.\n", PORT);
  fflush(stdout);
  int max_fd;
  for (int i = 0; i < MAX_CLIENTS; i++)
    memset(&clients[i], 0, sizeof(Client));

  while (1)
  {
    FD_ZERO(&active_fds);
    FD_SET(server_socket, &active_fds);
    max_fd = server_socket;

    // Add client descriptors to the set
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
      if (clients[i].to_close)
      {
        closeConnection(i);
      }
      if (clients[i].socket > 0)
      {
        FD_SET(clients[i].socket, &active_fds);
        if (clients[i].socket > max_fd)
          max_fd = clients[i].socket;
      }
    }

    // Check which descriptor is ready for reading
    if (0 > select(max_fd + 1, &active_fds, NULL, NULL, NULL))
      error("[Server] Error on select()");

    // Check if there is a new connection from a client
    if (FD_ISSET(server_socket, &active_fds))
    {
      if (connected_clients + additional_clients < MAX_CLIENTS)
      {
        if (-1 == (client_socket = accept(server_socket, (struct sockaddr *)&client, &len)))
          error("Error on accept()\n");

        // Add the new client to the client list
        clients[connected_clients + additional_clients].socket = client_socket;
        printf("A new client has connected\n");

        pthread_t thread;
        int *client_index = malloc(sizeof(int));
        *client_index = connected_clients + additional_clients;
        // pthread_create(&thread, NULL, processClient, client_index);

        fflush(stdout);
        connected_clients++;
      }
      else
      {
        printf("The maximum number of clients has been reached\n");
        fflush(stdout);
      }
    }
  }

  return 0;
}

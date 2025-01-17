#include <ctype.h>
#include <netinet/in.h>
#include <pthread.h>
#include "utils.h"
#include "database.h"

void *processClient(void *arg)
{
  int client_index = *((int *)arg);
  int client_socket = clients[client_index].socket;
  char buffer[MESSAGE_SIZE];

  while (1)
  {
    memset(buffer, 0, sizeof(buffer));
    if (recv(clients[client_index].socket, buffer, sizeof(buffer), 0) <= 0)
    {
      // The client has disconnected or an error occurred
      clients[client_index].to_close = 1;
      pthread_exit(NULL);
      break;
    }
    if (clients[client_index].username[0] == '\0')
    {
      // If no username has been received for this client yet, receive it now
      char temp_name[64];
      strncpy(temp_name, buffer, sizeof(temp_name) - 1);
      if (validateUser(temp_name))
      {
        int temp_id = getUserId(temp_name);
        if (isIdExisting(temp_id) == 0)
        {
          strcpy(clients[client_index].username, temp_name);
          clients[client_index].id = temp_id;

          printf("Client '%s' with ID '%d' has connected\n", clients[client_index].username, clients[client_index].id);

          char temp_message[MESSAGE_SIZE * 3];
          strcpy(temp_message, "Connected!\nMessage format: recipient_username:[<reply_message_id>] message\n");
          send(clients[client_index].socket, temp_message, sizeof(temp_message), 0);

          int messageCount;
          Message *unreadMessages = getUnreadMessages(temp_id, &messageCount);
          printf("Number of unread messages: %d\n", messageCount);

          for (int i = 0; i < messageCount; i++)
          {
            printf("Message[%d]: %s\n", i, unreadMessages[i].content);
            char *reply_message = getMessage(unreadMessages[i].replyId, getUserId(unreadMessages[i].sourceName));
            printf("ID: %d, Recipient ID: %d, Reply Message: %s\n", unreadMessages[i].replyId, temp_id, reply_message);

            if (unreadMessages[i].replyId > 0 && reply_message != NULL)
            {
              snprintf(temp_message, sizeof(temp_message), "→ Reply to message: %s\n  [%s]<%d>%s\n", reply_message, unreadMessages[i].sourceName, unreadMessages[i].id, unreadMessages[i].content);
              send(clients[client_index].socket, temp_message, strlen(temp_message), 0);
            }
            else
            {
              snprintf(temp_message, sizeof(temp_message), "[%s]<%d>%s \n", unreadMessages[i].sourceName, unreadMessages[i].id, unreadMessages[i].content);
              send(clients[client_index].socket, temp_message, strlen(temp_message), 0);
            }
          }
          free(unreadMessages);
        }
        else
        {
          printf("%d\n", temp_id);

          for (int k = 0; k < MAX_CLIENTS; k++)
          {
            printf("ID: %d, Name: %s\n", clients[k].id, clients[k].username);
          }
          char temp_message[128];
          strcpy(temp_message, "This user is already connected\nPlease enter a username: ");
          send(clients[client_index].socket, temp_message, sizeof(temp_message), 0);
        }
      }
      else
      {
        char temp_message[128];
        strcpy(temp_message, "Invalid username\nPlease enter a username: ");
        printf("Invalid username\n");
        send(clients[client_index].socket, temp_message, sizeof(temp_message), 0);
      }
      fflush(stdout);
    }
    else
    {
      if (strcmp(buffer, "history") == 0)
      {
        int messageCount;
        char temp_message[MESSAGE_SIZE * 3];
        Message *historyMessages = getHistory(clients[client_index].id, &messageCount);

        for (int i = 0; i < messageCount; i++)
        {
          if (historyMessages[i].replyId > 0)
          {
            char *reply_message = getHistoryMessage(historyMessages[i].replyId);
            if (strcmp(clients[client_index].username, historyMessages[i].sourceName) != 0)
            {
              snprintf(temp_message, sizeof(temp_message), "  → Reply to: %s\n  [%s]<%d>%s\n", reply_message, historyMessages[i].sourceName, historyMessages[i].id, historyMessages[i].content);
              send(clients[client_index].socket, temp_message, strlen(temp_message), 0);
            }
            else
            {
              snprintf(temp_message, sizeof(temp_message), "  → Reply to: %s\n  [me]<%d>%s\n", reply_message, historyMessages[i].id, historyMessages[i].content);
              send(clients[client_index].socket, temp_message, strlen(temp_message), 0);
            }
          }
          else
          {
            if (strcmp(clients[client_index].username, historyMessages[i].sourceName) != 0)
            {
              snprintf(temp_message, sizeof(temp_message), "[%s]<%d>%s \n", historyMessages[i].sourceName, historyMessages[i].id, historyMessages[i].content);
              send(clients[client_index].socket, temp_message, strlen(temp_message), 0);
            }
            else
            {
              snprintf(temp_message, sizeof(temp_message), "[me]<%d>%s \n", historyMessages[i].id, historyMessages[i].content);
              send(clients[client_index].socket, temp_message, strlen(temp_message), 0);
            }
          }
        }
        free(historyMessages);
        continue;
      }
      // Parse the received message
      char recipient_username[64], message[MESSAGE_SIZE];

      strcpy(recipient_username, extractRecipientUsername(buffer));
      strcpy(message, buffer);
      int reply_id = extractReplyId(message);
      printf("Message without reply: %s\n", message);

      int recipient_id = getUserId(recipient_username);
      int message_id = getMaxMessageId() + 1;

      // Check if the recipient is connected
      int resp_socket = -1;
      for (int j = 0; j < MAX_CLIENTS; j++)
      {
        if (strcmp(clients[j].username, recipient_username) == 0)
        {
          resp_socket = clients[j].socket;
          break;
        }
      }
      if (resp_socket == -1)
      {
        // The recipient is not connected
        if (validateUser(recipient_username))
        {
          storeMessage(clients[client_index].id, recipient_id, message, "Pending", reply_id);
        }
        else
        {
          // The recipient does not exist
          printf("Unknown recipient\n");
          fflush(stdout);
          char error_message[MESSAGE_SIZE + 50];
          snprintf(error_message, sizeof(error_message), "No user with the name '%s'.\n", recipient_username);
          send(clients[client_index].socket, error_message, strlen(error_message), 0);
        }
      }
      else
      {
        // The recipient is connected, send the message
        printf("%s -> %s : %s\n", clients[client_index].username, recipient_username, message);
        fflush(stdout);

        storeMessage(clients[client_index].id, recipient_id, message, "Delivered", reply_id);

        char temp_message[MESSAGE_SIZE + 128];
        char *reply_message;

        if (reply_id > 0 && (reply_message = getMessage(reply_id, clients[client_index].id)) != NULL)
        {
          snprintf(temp_message, sizeof(temp_message), "→ Reply to message: %s\n  [%s]<%d>%s\n", reply_message, clients[client_index].username, message_id, message);
          send(resp_socket, temp_message, strlen(temp_message), 0);
        }
        else
        {
          snprintf(temp_message, sizeof(temp_message), "[%s]<%d>%s \n", clients[client_index].username, message_id, message);
          send(resp_socket, temp_message, strlen(temp_message), 0);
        }
      }
    }
  }
}
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

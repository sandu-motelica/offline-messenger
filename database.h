#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <pthread.h>

sqlite3 *db;

typedef struct
{
    int id;
    char sourceName[64];
    int replyId;
    char content[512];
} Message;

int execute_sql_command(const char *sql)
{
    char *error_message;
    int response = sqlite3_exec(db, sql, 0, 0, &error_message);
    if (response != SQLITE_OK)
    {
        fprintf(stderr, "SQL execution error: %s\n", error_message);
        sqlite3_free(error_message);
    }
    return response;
}

int validateUser(const char *username)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT COUNT(*) FROM Users WHERE Username = ?";
    int response = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    if (response != SQLITE_OK)
    {
        fprintf(stderr, "SQL query preparation error: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    // Bind the parameter to the query
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    // Execute the query
    response = sqlite3_step(stmt);
    if (response != SQLITE_ROW)
    {
        fprintf(stderr, "SQL query execution error: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }

    // Retrieve the result
    int userCount = sqlite3_column_int(stmt, 0);

    // Finalize the query
    sqlite3_finalize(stmt);

    return userCount;
}

int getUserId(const char *username)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id FROM Users WHERE Username = ?";
    int userId = -1;

    int response = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    if (response != SQLITE_OK)
    {
        fprintf(stderr, "SQL query preparation error: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    // Bind the parameter to the query
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    // Execute the query
    response = sqlite3_step(stmt);
    if (response == SQLITE_ROW)
    {
        // Retrieve the result
        userId = sqlite3_column_int(stmt, 0);
    }
    else if (response != SQLITE_DONE)
    {
        fprintf(stderr, "SQL query execution error: %s\n", sqlite3_errmsg(db));
    }

    // Finalize the query
    sqlite3_finalize(stmt);

    return userId;
}

void storeMessage(int sourceId, int recipientId, const char *content, const char *status, int replyId)
{
    char *command = (char *)malloc(1024);

    snprintf(command, 1024,
             "INSERT INTO Messages (SourceUserId, RecipientUserId, Content, Status, Reply) VALUES (%d, %d, '%s', '%s', %d);",
             sourceId, recipientId, content, status, replyId);

    execute_sql_command(command);

    free(command);
    printf("(%d, %d, '%s', '%s', %d) - stored successfully\n", sourceId, recipientId, content, status, replyId);
}

int getMaxId()
{
    sqlite3_stmt *query_ptr;
    const char *sql = "SELECT MAX(ID) FROM Messages";
    int maxId = -1;

    int response = sqlite3_prepare_v2(db, sql, -1, &query_ptr, NULL);

    if (response != SQLITE_OK)
    {
        fprintf(stderr, "SQL query preparation error: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    response = sqlite3_step(query_ptr);
    if (response != SQLITE_ROW)
    {
        fprintf(stderr, "SQL query execution error: %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }
    else
        maxId = sqlite3_column_int(query_ptr, 0);

    sqlite3_finalize(query_ptr);

    return maxId;
}

Message *getUnreadMessages(int recipientId, int *messageCount)
{
    char command[512];
    snprintf(command, sizeof(command), "SELECT m.ID, u.Username, m.Reply, m.Content FROM Messages m JOIN Users u ON m.SourceUserId=u.ID WHERE Status = 'Pending' AND RecipientUserId = %d ORDER BY m.ID ASC;", recipientId);

    sqlite3_stmt *query_ptr;
    int response = sqlite3_prepare_v2(db, command, -1, &query_ptr, 0);
    if (response != SQLITE_OK)
    {
        fprintf(stderr, "SQL statement preparation error.\n");
        return NULL;
    }

    int resultCount = 0;
    Message *results = NULL;

    while (sqlite3_step(query_ptr) == SQLITE_ROW)
    {
        resultCount++;

        results = (Message *)realloc(results, resultCount * sizeof(Message));

        results[resultCount - 1].id = sqlite3_column_int(query_ptr, 0);
        strcpy(results[resultCount - 1].sourceName, sqlite3_column_text(query_ptr, 1));
        results[resultCount - 1].replyId = sqlite3_column_int(query_ptr, 2);
        strcpy(results[resultCount - 1].content, sqlite3_column_text(query_ptr, 3));
    }

    *messageCount = resultCount;
    sqlite3_finalize(query_ptr);

    sqlite3_stmt *update_query;
    snprintf(command, sizeof(command), "UPDATE Messages SET Status='Delivered' WHERE Status='Pending' AND RecipientUserId=%d;", recipientId);
    response = sqlite3_prepare_v2(db, command, -1, &update_query, 0);

    if (response != SQLITE_OK)
    {
        fprintf(stderr, "SQL statement preparation error.\n");
        return NULL;
    }
    response = sqlite3_step(update_query);
    if (response != SQLITE_DONE)
    {
        fprintf(stderr, "SQL update statement execution error.\n");
        sqlite3_finalize(update_query);
        free(results);
        return NULL;
    }

    sqlite3_finalize(update_query);

    return results;
}

char *getMessageContent(int messageId, int recipientId)
{
    char command[1024];
    snprintf(command, sizeof(command), "SELECT Content FROM Messages WHERE ID = %d AND RecipientUserId = %d;", messageId, recipientId);

    sqlite3_stmt *query_ptr;
    int response = sqlite3_prepare_v2(db, command, -1, &query_ptr, 0);
    if (response != SQLITE_OK)
    {
        fprintf(stderr, "SQL statement preparation error for message content retrieval.\n");
        return NULL;
    }

    char *content = NULL;

    if (sqlite3_step(query_ptr) == SQLITE_ROW)
    {
        const char *contentText = (const char *)sqlite3_column_text(query_ptr, 0);
        content = strdup(contentText); // Duplicate content to return it
    }

    sqlite3_finalize(query_ptr);

    return content;
}

Message *getMessageHistory(int clientId, int *messageCount)
{
    char command[512];
    snprintf(command, sizeof(command),
             "SELECT m.ID, u.Username, m.Reply, m.Content "
             "FROM Messages m JOIN Users u ON m.SourceUserId = u.ID "
             "WHERE m.SourceUserId = %d OR m.RecipientUserId = %d "
             "ORDER BY m.ID ASC;",
             clientId, clientId);

    sqlite3_stmt *query_ptr;
    int response = sqlite3_prepare_v2(db, command, -1, &query_ptr, 0);
    if (response != SQLITE_OK)
    {
        fprintf(stderr, "SQL statement preparation error for message history retrieval.\n");
        return NULL;
    }

    int count = 0;
    Message *results = NULL;

    while (sqlite3_step(query_ptr) == SQLITE_ROW)
    {
        count++;

        results = (Message *)realloc(results, count * sizeof(Message));

        results[count - 1].id = sqlite3_column_int(query_ptr, 0);
        strcpy(results[count - 1].sourceName, sqlite3_column_text(query_ptr, 1));
        results[count - 1].replyId = sqlite3_column_int(query_ptr, 2);
        strcpy(results[count - 1].content, sqlite3_column_text(query_ptr, 3));
    }

    *messageCount = count;
    sqlite3_finalize(query_ptr);

    return results;
}

char *getHistoricalMessage(int messageId)
{
    char command[1024];
    snprintf(command, sizeof(command), "SELECT Content FROM Messages WHERE ID = %d;", messageId);

    sqlite3_stmt *query_ptr;
    int response = sqlite3_prepare_v2(db, command, -1, &query_ptr, 0);
    if (response != SQLITE_OK)
    {
        fprintf(stderr, "SQL statement preparation error for message content retrieval.\n");
        return NULL;
    }

    char *content = NULL;

    if (sqlite3_step(query_ptr) == SQLITE_ROW)
    {
        const char *contentText = (const char *)sqlite3_column_text(query_ptr, 0);
        content = strdup(contentText); // Duplicate content to return it
    }

    sqlite3_finalize(query_ptr);

    return content;
}

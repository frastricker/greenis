#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define PORT 6379
#define BUFFER_SIZE 4096

typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
} connection_t;

typedef struct {
    char *key;
    char *value;
    time_t expiration;
} kv_store_t;

kv_store_t store[256];
pthread_mutex_t store_mutex = PTHREAD_MUTEX_INITIALIZER;

void exit_with_error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void *connection_handler(void *data) {
    connection_t *conn = (connection_t *)data;
    int client_socket = conn->client_socket;
    struct sockaddr_in client_addr = conn->client_addr;

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    // Read request from client
    ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        close(client_socket);
        return NULL;
    }

    // Process request
    char command[8];
    sscanf(buffer, "*%s", command);

    if (strcmp(command, "get") == 0) {
        char key[256];
        sscanf(buffer, "*%s%s", command, key);

        pthread_mutex_lock(&store_mutex);
        for (int i = 0; i < 256; i++) {
            if (store[i].key && strcmp(store[i].key, key) == 0 && store[i].expiration > time(NULL)) {
                send(client_socket, "*$%s\r\n$%s\r\n", store[i].value, strlen(store[i].value));
                break;
            }
        }
        pthread_mutex_unlock(&store_mutex);

    } else if (strcmp(command, "set") == 0) {
        char key[256], value[256];
        int expiration = -1;

        sscanf(buffer, "*%s%s%s%d", command, key, value, &expiration);

        pthread_mutex_lock(&store_mutex);
        for (int i = 0; i < 256; i++) {
            if (store[i].key && strcmp(store[i].key, key) == 0) {
                free(store[i].key);
                free(store[i].value);
                store[i].key = NULL;
                store[i].value = NULL;
            }

            if (!store[i].key) {
                store[i].key = strdup(key);
                store[i].value = strdup(value);
                store[i].expiration = expiration > -1? time(NULL) + expiration : -1;
                break;
            }
        }
        pthread_mutex_unlock(&store_mutex);

        send(client_socket, "+OK\r\n", 4, 0);
    }

    close(client_socket);
    return NULL;
}

int main(int argc, const char *argv[]) {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;

    socklen_t sin_size = sizeof(struct sockaddr_in);

    if (argc!= 2) {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        exit_with_error("socket");
    }

    memset((char *)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        exit_with_error("inet_pton");
    }

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        exit_with_error("bind");
    }

    if (listen(server_socket, 5) == -1) {
        exit_with_error("listen");
    }

    printf("Listening on port %d...\n", PORT);

    while (1) {
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &sin_size)) < 0) {
            exit_with_error("accept");
        }

        connection_t *conn = malloc(sizeof(connection_t));
        conn->client_socket = client_socket;
        conn->client_addr = client_addr;

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, connection_handler, (void *)conn)!= 0) {
            exit_with_error("pthread_create");
        }
    }

close(server_socket);
    return EXIT_SUCCESS;
}